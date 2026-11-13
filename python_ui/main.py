"""Fairy Chess - PyQt6 GUI. All game rules live in the C++ engine; this file only shows them."""
from __future__ import annotations

import os
import sys
from typing import Optional

from PyQt6.QtGui import QFont
from PyQt6.QtWidgets import (
    QApplication, QHBoxLayout, QLabel, QListWidget, QMainWindow,
    QMessageBox, QPushButton, QVBoxLayout, QWidget,
)

from board_widget import ChessBoardWidget
from engine_adapter import EngineAdapter, EngineError
from promotion_dialog import PromotionDialog

PIECES_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "Pieces")

GLYPHS = {
    "wK": "♔", "wQ": "♕", "wR": "♖", "wB": "♗", "wN": "♘", "wP": "♙",
    "bK": "♚", "bQ": "♛", "bR": "♜", "bB": "♝", "bN": "♞", "bP": "♟",
}


class MainWindow(QMainWindow):
    def __init__(self, adapter: EngineAdapter):
        super().__init__()
        self.setWindowTitle("Fairy Chess")
        self.resize(1000, 700)
        self._adapter = adapter
        self._state: Optional[dict] = None
        self._init_ui()
        self._start_engine()

    # ------------------------------------------------------------ UI

    def _init_ui(self) -> None:
        central = QWidget()
        self.setCentralWidget(central)
        layout = QHBoxLayout(central)

        self.board_widget = ChessBoardWidget(PIECES_DIR)
        self.board_widget.move_requested.connect(self._on_move_requested)
        self.board_widget.selection_changed.connect(self._on_selection_changed)
        layout.addWidget(self.board_widget, stretch=3)

        side = QVBoxLayout()
        side.addWidget(QLabel("Mode: Player vs Player (Hotseat)"))

        self.new_game_btn = QPushButton("New Game")
        self.new_game_btn.clicked.connect(self._on_new_game)
        side.addWidget(self.new_game_btn)

        self.status_label = QLabel()
        status_font = QFont(self.font())
        status_font.setPointSize(status_font.pointSize() + 3)
        status_font.setBold(True)
        self.status_label.setFont(status_font)
        self.status_label.setWordWrap(True)
        side.addWidget(self.status_label)

        side.addWidget(QLabel("Move History:"))
        self.history_list = QListWidget()
        mono = QFont("Consolas")
        mono.setStyleHint(QFont.StyleHint.Monospace)
        self.history_list.setFont(mono)
        side.addWidget(self.history_list, stretch=1)

        buttons = QHBoxLayout()
        self.undo_btn = QPushButton("Undo")
        self.redo_btn = QPushButton("Redo")
        self.undo_btn.clicked.connect(self._on_undo)
        self.redo_btn.clicked.connect(self._on_redo)
        buttons.addWidget(self.undo_btn)
        buttons.addWidget(self.redo_btn)
        side.addLayout(buttons)

        self.material_label = QLabel()
        self.captured_white_label = QLabel()
        self.captured_black_label = QLabel()
        glyph_font = QFont(self.font())
        glyph_font.setPointSize(glyph_font.pointSize() + 4)
        for label in (self.captured_white_label, self.captured_black_label):
            label.setFont(glyph_font)
            label.setWordWrap(True)
        side.addWidget(self.material_label)
        side.addWidget(self.captured_white_label)
        side.addWidget(self.captured_black_label)

        side.addWidget(QLabel("Selected piece:"))
        self.piece_info = QLabel("Click any piece to inspect it.")
        self.piece_info.setWordWrap(True)
        side.addWidget(self.piece_info)

        layout.addLayout(side, stretch=1)
        self._set_controls_enabled(False)

    def _set_controls_enabled(self, enabled: bool) -> None:
        can_undo = enabled and bool(self._state and self._state.get("can_undo"))
        can_redo = enabled and bool(self._state and self._state.get("can_redo"))
        self.undo_btn.setEnabled(can_undo)
        self.redo_btn.setEnabled(can_redo)
        self.new_game_btn.setEnabled(enabled)
        self.board_widget.set_interactive(enabled)

    # ------------------------------------------------------------ engine communication

    def _start_engine(self) -> None:
        try:
            self._adapter.start()
            self._apply(self._adapter.send({"cmd": "new_game"}))
        except EngineError as exc:
            self._engine_unavailable(str(exc))

    def _request(self, payload: dict) -> bool:
        """Fast synchronous request (moves, undo...). Returns True if the engine accepted it."""
        try:
            response = self._adapter.send(payload)
        except EngineError as exc:
            self._engine_failed(str(exc))
            return False
        if response.get("status") != "ok":
            self.statusBar().showMessage(response.get("message", "Engine error"), 4000)
            return False
        self._apply(response)
        return True

    def _engine_failed(self, message: str) -> None:
        QMessageBox.critical(self, "Engine error",
                             f"{message}\n\nThe engine will be restarted with a new game.")
        try:
            self._adapter.restart()
            self._apply(self._adapter.send({"cmd": "new_game"}))
        except EngineError as exc:
            self._engine_unavailable(str(exc))

    def _engine_unavailable(self, message: str) -> None:
        self._state = None
        self._set_controls_enabled(False)
        self.status_label.setText("Engine unavailable")
        QMessageBox.critical(self, "Engine error", message)

    # ------------------------------------------------------------ state -> widgets

    def _apply(self, state: dict) -> None:
        self._state = state
        self.board_widget.set_state(state)
        self._update_history(state.get("history", []))
        self.status_label.setText(self._status_text(state))

        material = state.get("material", {})
        self.material_label.setText(
            f"Material: White {material.get('white', 0)}  ·  Black {material.get('black', 0)}")
        captured = state.get("captured", {})
        self.captured_white_label.setText(
            "White took: " + " ".join(GLYPHS.get(c, c) for c in captured.get("white", [])))
        self.captured_black_label.setText(
            "Black took: " + " ".join(GLYPHS.get(c, c) for c in captured.get("black", [])))

        self._set_controls_enabled(True)

    @staticmethod
    def _status_text(state: dict) -> str:
        turn = state.get("turn", "white").capitalize()
        other = "Black" if turn == "White" else "White"
        texts = {
            "playing": f"{turn} to move",
            "check": f"{turn} to move — Check!",
            "checkmate": f"Checkmate! {other} wins ({state.get('result', '')})",
            "stalemate": "Stalemate — draw",
            "draw_repetition": "Draw by threefold repetition",
            "draw_fifty_moves": "Draw by the fifty-move rule",
        }
        return texts.get(state.get("state", ""), state.get("state", ""))

    def _update_history(self, san: list) -> None:
        self.history_list.clear()
        for i in range(0, len(san), 2):
            black = san[i + 1] if i + 1 < len(san) else ""
            self.history_list.addItem(f"{i // 2 + 1:>3}. {san[i]:<9}{black}")
        self.history_list.scrollToBottom()

    def _on_selection_changed(self, piece: Optional[dict]) -> None:
        if piece is None:
            self.piece_info.setText("Click any piece to inspect it.")
            return
        path = piece.get("path", [])
        shown = (["…"] + path[-8:]) if len(path) > 8 else path
        route = " → ".join(shown)
        self.piece_info.setText(
            f"{piece['color'].capitalize()} {piece['name']} (id {piece['id']})\n"
            f"Value: {piece['value']}   Moves made: {piece['moves']}\n"
            f"Path: {route}"
        )

    # ------------------------------------------------------------ user actions

    def _on_move_requested(self, from_sq: int, to_sq: int) -> None:
        if not self._state:
            return
        candidates = [m for m in self._state.get("legal_moves", [])
                      if m["from"] == from_sq and m["to"] == to_sq]
        promo = ""
        if any(m["promo"] for m in candidates):
            promo = PromotionDialog.ask(self._state["turn"], PIECES_DIR, self) or ""
            if not promo:
                return  # dialog closed - move cancelled
        self._request({"cmd": "move", "from": from_sq, "to": to_sq, "promo": promo})

    def _on_new_game(self) -> None:
        self._request({"cmd": "new_game"})

    def _on_undo(self) -> None:
        self._request({"cmd": "undo"})

    def _on_redo(self) -> None:
        self._request({"cmd": "redo"})

    def closeEvent(self, event) -> None:
        self._adapter.stop()
        super().closeEvent(event)


def main() -> int:
    app = QApplication(sys.argv)
    window = MainWindow(EngineAdapter())
    window.show()
    return app.exec()


if __name__ == "__main__":
    sys.exit(main())
