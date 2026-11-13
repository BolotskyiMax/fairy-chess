"""Small modal dialog that asks which piece a pawn promotes to."""
from __future__ import annotations

import os
from typing import Optional

from PyQt6.QtCore import QSize
from PyQt6.QtGui import QIcon
from PyQt6.QtWidgets import QDialog, QHBoxLayout, QPushButton

CHOICES = [("Q", "Queen"), ("R", "Rook"), ("B", "Bishop"), ("N", "Knight")]


class PromotionDialog(QDialog):
    def __init__(self, color: str, pieces_dir: str, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Promote pawn to...")
        self.choice: Optional[str] = None

        layout = QHBoxLayout(self)
        prefix = "w" if color == "white" else "b"
        for letter, name in CHOICES:
            button = QPushButton()
            icon_path = os.path.join(pieces_dir, f"{prefix}{letter}.svg")
            if os.path.exists(icon_path):
                button.setIcon(QIcon(icon_path))
                button.setIconSize(QSize(56, 56))
            else:
                button.setText(name)
            button.setToolTip(name)
            button.clicked.connect(lambda _checked, l=letter: self._choose(l))
            layout.addWidget(button)

    def _choose(self, letter: str) -> None:
        self.choice = letter
        self.accept()

    @classmethod
    def ask(cls, color: str, pieces_dir: str, parent=None) -> Optional[str]:
        dialog = cls(color, pieces_dir, parent)
        dialog.exec()
        return dialog.choice
