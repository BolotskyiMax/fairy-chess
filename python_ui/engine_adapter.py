"""Adapter between the PyQt6 GUI and the C++ engine process.

The engine speaks line-delimited JSON over stdin/stdout. This class hides the
subprocess details behind one method, ``send(payload) -> dict``, so the GUI does
not care whether the engine is a local process (now) or e.g. a REST server (later).
"""
from __future__ import annotations

import json
import os
import queue
import subprocess
import threading
from collections import deque
from typing import Optional


class EngineError(RuntimeError):
    """The engine process cannot be reached, crashed, timed out or sent garbage."""


def default_engine_path() -> str:
    project_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    name = "chess_engine.exe" if os.name == "nt" else "chess_engine"
    return os.path.join(project_dir, "cpp_engine", name)


class EngineAdapter:
    DEFAULT_TIMEOUT = 5.0  # seconds

    def __init__(self, executable_path: Optional[str] = None):
        self._path = executable_path or default_engine_path()
        self._process: Optional[subprocess.Popen] = None
        self._lines: "queue.Queue[Optional[str]]" = queue.Queue()
        self._stderr_tail: deque = deque(maxlen=20)
        self._lock = threading.Lock()  # one request at a time, even from several threads

    # ------------------------------------------------------------ lifecycle

    def start(self) -> None:
        if not os.path.exists(self._path):
            raise EngineError(
                f"Engine executable not found:\n{self._path}\n\nBuild it first by running run.bat."
            )
        flags = subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0  # no extra console window
        self._lines = queue.Queue()
        self._stderr_tail.clear()
        try:
            self._process = subprocess.Popen(
                [self._path],
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                encoding="utf-8",
                bufsize=1,
                creationflags=flags,
            )
        except OSError as exc:
            raise EngineError(f"Cannot start the engine:\n{exc}") from exc

        # Reader threads: reading never blocks the GUI, and a dead engine is noticed at once.
        threading.Thread(target=self._read_stdout, args=(self._process, self._lines), daemon=True).start()
        threading.Thread(target=self._read_stderr, args=(self._process,), daemon=True).start()

    def stop(self) -> None:
        process, self._process = self._process, None
        if process is None:
            return
        if process.poll() is None:
            try:
                process.stdin.write(json.dumps({"cmd": "quit"}) + "\n")
                process.stdin.flush()
                process.wait(timeout=1)
            except (OSError, subprocess.TimeoutExpired):
                self._kill(process)

    def restart(self) -> None:
        self.stop()
        self.start()

    def is_running(self) -> bool:
        return self._process is not None and self._process.poll() is None

    # ------------------------------------------------------------ requests

    def send(self, payload: dict, timeout: Optional[float] = None) -> dict:
        """Sends one request and returns the engine's JSON answer.

        Raises EngineError for transport problems. Rule errors (e.g. an illegal
        move) come back as a normal answer with ``"status": "error"``.
        """
        timeout = timeout or self.DEFAULT_TIMEOUT
        command = payload.get("cmd", "?")
        with self._lock:
            if not self.is_running():
                raise EngineError(self._describe_failure("The engine is not running."))
            try:
                self._process.stdin.write(json.dumps(payload) + "\n")
                self._process.stdin.flush()
            except OSError as exc:
                raise EngineError(self._describe_failure(f"Cannot write to the engine: {exc}")) from exc

            try:
                line = self._lines.get(timeout=timeout)
            except queue.Empty:
                self._kill(self._process)
                raise EngineError(
                    f"The engine did not answer '{command}' within {timeout:.0f} s and was stopped."
                ) from None

            if line is None:
                raise EngineError(self._describe_failure("The engine closed unexpectedly."))
            try:
                return json.loads(line)
            except json.JSONDecodeError as exc:
                raise EngineError(f"The engine sent invalid JSON:\n{line[:300]}") from exc

    # ------------------------------------------------------------ helpers

    @staticmethod
    def _read_stdout(process: subprocess.Popen, sink: "queue.Queue[Optional[str]]") -> None:
        for line in process.stdout:
            sink.put(line.rstrip("\r\n"))
        sink.put(None)  # end of stream: wakes up a waiting send() immediately

    def _read_stderr(self, process: subprocess.Popen) -> None:
        for line in process.stderr:
            self._stderr_tail.append(line.rstrip("\r\n"))

    @staticmethod
    def _kill(process: Optional[subprocess.Popen]) -> None:
        if process is None:
            return
        try:
            process.kill()
            process.wait(timeout=1)
        except (OSError, subprocess.TimeoutExpired):
            pass

    def _describe_failure(self, message: str) -> str:
        parts = [message]
        if self._process is not None and self._process.poll() is not None:
            parts.append(f"Exit code: {self._process.returncode}")
        if self._stderr_tail:
            parts.append("Engine output:\n" + "\n".join(self._stderr_tail))
        return "\n\n".join(parts)
