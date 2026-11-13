"""Chess board widget: draws the position sent by the engine and turns clicks into move requests.

The widget never decides what is legal - it only shows the ``legal_moves`` list the engine sent.
"""
from __future__ import annotations

import os
from typing import Optional

from PyQt6.QtCore import QPointF, QRectF, Qt, pyqtSignal
from PyQt6.QtGui import QBrush, QColor, QFont, QPainter, QPen, QRadialGradient
from PyQt6.QtSvg import QSvgRenderer
from PyQt6.QtWidgets import QWidget

PIECE_CODES = ["wP", "wN", "wB", "wR", "wQ", "wK", "bP", "bN", "bB", "bR", "bQ", "bK"]


class ChessBoardWidget(QWidget):
    move_requested = pyqtSignal(int, int)    # from square, to square
    selection_changed = pyqtSignal(object)   # piece dict from the engine, or None

    LIGHT = QColor("#EBECD0")
    DARK = QColor("#739552")
    LAST_MOVE = QColor(255, 255, 51, 110)
    SELECTED = QColor(20, 85, 30, 120)
    TARGET = QColor(20, 85, 30, 110)

    def __init__(self, pieces_dir: str, parent=None):
        super().__init__(parent)
        self.setMinimumSize(480, 480)
        self._renderers = self._load_svgs(pieces_dir)
        self._board: list = [None] * 64
        self._legal: list = []
        self._last_move: Optional[dict] = None
        self._check_square = -1
        self._selected: Optional[int] = None
        self._interactive = True

    @staticmethod
    def _load_svgs(pieces_dir: str) -> dict:
        renderers = {}
        for code in PIECE_CODES:
            path = os.path.join(pieces_dir, f"{code}.svg")
            if os.path.exists(path):
                renderers[code] = QSvgRenderer(path)
        return renderers

    # ------------------------------------------------------------ public API

    def set_state(self, state: dict) -> None:
        previous = self._piece(self._selected) if self._selected is not None else None
        self._board = state.get("board", [None] * 64)
        self._legal = state.get("legal_moves", [])
        self._last_move = state.get("last_move")
        self._check_square = state.get("check_square", -1)

        # Keep the selection only if the same piece is still on that square.
        current = self._piece(self._selected) if self._selected is not None else None
        if previous is None or current is None or previous.get("id") != current.get("id"):
            self.clear_selection()
        else:
            self.selection_changed.emit(current)
        self.update()

    def set_interactive(self, interactive: bool) -> None:
        self._interactive = interactive
        self.update()

    def clear_selection(self) -> None:
        if self._selected is not None:
            self._selected = None
            self.selection_changed.emit(None)
            self.update()

    # ------------------------------------------------------------ geometry

    def _geometry(self) -> tuple[float, float, float]:
        size = min(self.width(), self.height()) - 28
        square = size / 8.0
        x0 = (self.width() - size) / 2.0 + 8
        y0 = (self.height() - size) / 2.0 - 4
        return x0, y0, square

    def _square_rect(self, sq: int) -> QRectF:
        x0, y0, s = self._geometry()
        return QRectF(x0 + (sq % 8) * s, y0 + (sq // 8) * s, s, s)

    def _square_at(self, pos: QPointF) -> Optional[int]:
        x0, y0, s = self._geometry()
        col = int((pos.x() - x0) // s)
        row = int((pos.y() - y0) // s)
        if 0 <= row < 8 and 0 <= col < 8:
            return row * 8 + col
        return None

    def _piece(self, sq: Optional[int]) -> Optional[dict]:
        return self._board[sq] if sq is not None and 0 <= sq < 64 else None

    def _targets(self) -> set:
        if self._selected is None or not self._interactive:
            return set()
        return {m["to"] for m in self._legal if m["from"] == self._selected}

    # ------------------------------------------------------------ input

    def mousePressEvent(self, event) -> None:
        if event.button() == Qt.MouseButton.RightButton:
            self.clear_selection()
            return
        if event.button() != Qt.MouseButton.LeftButton:
            return

        sq = self._square_at(event.position())
        if sq is None:
            self.clear_selection()
            return
        if sq in self._targets():
            origin = self._selected
            self.clear_selection()
            self.move_requested.emit(origin, sq)
            return
        if sq == self._selected or self._piece(sq) is None:
            self.clear_selection()
            return

        # Any piece can be inspected; only the side to move gets move markers.
        self._selected = sq
        self.selection_changed.emit(self._piece(sq))
        self.update()

    # ------------------------------------------------------------ painting

    def paintEvent(self, event) -> None:
        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)
        targets = self._targets()

        for sq in range(64):
            rect = self._square_rect(sq)
            row, col = divmod(sq, 8)
            painter.fillRect(rect, self.LIGHT if (row + col) % 2 == 0 else self.DARK)

            if self._last_move and sq in (self._last_move["from"], self._last_move["to"]):
                painter.fillRect(rect, self.LAST_MOVE)
            if sq == self._selected:
                painter.fillRect(rect, self.SELECTED)
            if sq == self._check_square:
                self._draw_check(painter, rect)

            piece = self._board[sq]
            if piece and piece["code"] in self._renderers:
                m = rect.width() * 0.06
                self._renderers[piece["code"]].render(painter, rect.adjusted(m, m, -m, -m))

            if sq in targets:
                self._draw_target(painter, rect, capture=piece is not None)

        self._draw_coordinates(painter)

    @staticmethod
    def _draw_check(painter: QPainter, rect: QRectF) -> None:
        gradient = QRadialGradient(rect.center(), rect.width() * 0.6)
        gradient.setColorAt(0.0, QColor(255, 0, 0, 220))
        gradient.setColorAt(1.0, QColor(255, 0, 0, 0))
        painter.fillRect(rect, QBrush(gradient))

    def _draw_target(self, painter: QPainter, rect: QRectF, capture: bool) -> None:
        painter.save()
        if capture:
            pen = QPen(self.TARGET, rect.width() * 0.08)
            painter.setPen(pen)
            painter.setBrush(Qt.BrushStyle.NoBrush)
            r = rect.width() * 0.44
            painter.drawEllipse(rect.center(), r, r)
        else:
            painter.setPen(Qt.PenStyle.NoPen)
            painter.setBrush(self.TARGET)
            r = rect.width() * 0.15
            painter.drawEllipse(rect.center(), r, r)
        painter.restore()

    def _draw_coordinates(self, painter: QPainter) -> None:
        x0, y0, s = self._geometry()
        painter.setPen(self.palette().windowText().color())
        font = QFont(self.font())
        font.setPointSizeF(max(8.0, s * 0.16))
        painter.setFont(font)
        for i in range(8):
            painter.drawText(QRectF(x0 + i * s, y0 + 8 * s, s, 18),
                             Qt.AlignmentFlag.AlignCenter, "abcdefgh"[i])
            painter.drawText(QRectF(x0 - 18, y0 + i * s, 16, s),
                             Qt.AlignmentFlag.AlignCenter, str(8 - i))
