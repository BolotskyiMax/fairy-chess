"""Demo of the C++ <-> Python call: starts chess_engine.exe and exchanges a few JSON messages.

Run (after run.bat has built the engine):  python python_ui\\demo_ipc.py
"""
from __future__ import annotations

import json

from engine_adapter import EngineAdapter, EngineError


def show(title: str, request: dict, response: dict) -> None:
    print(f"\n=== {title}")
    print("Python -> C++:", json.dumps(request))
    if response.get("status") != "ok":
        print("C++ -> Python:", json.dumps(response))
        return
    summary = {
        "status": response["status"],
        "state": response["state"],
        "turn": response["turn"],
        "history": response["history"],
        "legal_moves": f"{len(response['legal_moves'])} moves",
    }
    print("C++ -> Python:", json.dumps(summary), " (board and move list shortened)")


def main() -> int:
    adapter = EngineAdapter()
    try:
        adapter.start()
        steps = [
            ("New game", {"cmd": "new_game"}),
            ("Legal move e2-e4 (square 52 -> 36)", {"cmd": "move", "from": 52, "to": 36, "promo": ""}),
            ("Illegal move: black pawn e7-e4", {"cmd": "move", "from": 12, "to": 36, "promo": ""}),
            ("Undo", {"cmd": "undo"}),
            ("Unknown command", {"cmd": "fly"}),
        ]
        for title, request in steps:
            show(title, request, adapter.send(request))
    except EngineError as exc:
        print("Engine error:", exc)
        return 1
    finally:
        adapter.stop()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
