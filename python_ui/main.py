import os
import sys
import json
import subprocess
from PyQt6.QtCore import Qt, QRectF
from PyQt6.QtGui import QPainter, QColor
from PyQt6.QtSvg import QSvgRenderer
from PyQt6.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QPushButton, QLabel, QComboBox, QListWidget
)


class EngineAdapter:
    def __init__(self):
        base_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
        self.executable_path = os.path.join(base_dir, "cpp_engine", "chess_engine.exe")
        self.process = None
        self._start_engine()

    def _start_engine(self):
        if os.path.exists(self.executable_path):
            self.process = subprocess.Popen(
                [self.executable_path],
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                text=True,
                bufsize=1
            )

    def send_command(self, payload: dict) -> dict:
        if self.process and self.process.poll() is None:
            try:
                self.process.stdin.write(json.dumps(payload) + "\n")
                self.process.stdin.flush()
                line = self.process.stdout.readline().strip()
                if line:
                    return json.loads(line)
            except Exception:
                pass
        preset = payload.get("preset", 0)
        if preset == 0:
            return {
                "status": "ok",
                "bitboards": {
                    "bR": (1 << 0) | (1 << 7),
                    "bN": (1 << 1) | (1 << 6),
                    "bB": (1 << 2) | (1 << 5),
                    "bQ": (1 << 3),
                    "bK": (1 << 4),
                    "bP": 0x000000000000FF00,
                    "wP": 0x00FF000000000000,
                    "wR": (1 << 56) | (1 << 63),
                    "wN": (1 << 57) | (1 << 62),
                    "wB": (1 << 58) | (1 << 61),
                    "wQ": (1 << 59),
                    "wK": (1 << 60),
                }
            }
        else:
            return {
                "status": "ok",
                "bitboards": {
                    "bN": (1 << 0) | (1 << 7),
                    "bR": (1 << 1) | (1 << 6),
                    "bB": (1 << 2) | (1 << 5) | (1 << 11) | (1 << 12),
                    "bQ": (1 << 3),
                    "bK": (1 << 4),
                    "bP": 0x000000000000E700,
                    "wP": 0x00E7000000000000,
                    "wN": (1 << 56) | (1 << 63),
                    "wR": (1 << 57) | (1 << 62),
                    "wB": (1 << 58) | (1 << 61) | (1 << 51) | (1 << 52),
                    "wQ": (1 << 59),
                    "wK": (1 << 60),
                }
            }


class ChessBoardWidget(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setMinimumSize(640, 640)
        self.bitboards: dict[str, int] = {}
        self.svg_cache: dict[str, QSvgRenderer] = {}
        self._load_svgs()

    def _load_svgs(self):
        pieces_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "Pieces")
        for code in ["wP", "wN", "wB", "wR", "wQ", "wK", "bP", "bN", "bB", "bR", "bQ", "bK"]:
            path = os.path.join(pieces_dir, f"{code}.svg")
            if os.path.exists(path):
                self.svg_cache[code] = QSvgRenderer(path)

    def set_bitboards(self, bitboards: dict[str, int]):
        self.bitboards = {k: int(v) for k, v in bitboards.items()}
        self.update()

    def _get_piece_at_square(self, sq_index: int) -> str:
        mask = 1 << sq_index
        for piece_code, bb in self.bitboards.items():
            if bb & mask:
                return piece_code
        return ""

    def paintEvent(self, event):
        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)

        board_size = min(self.width(), self.height()) - 24
        sq_size = board_size / 8.0
        offset_x = (self.width() - board_size) / 2.0
        offset_y = (self.height() - board_size) / 2.0

        light_color = QColor("#EBECD0")
        dark_color = QColor("#739552")

        for r in range(8):
            for c in range(8):
                sq_index = r * 8 + c
                x = offset_x + c * sq_size
                y = offset_y + r * sq_size
                rect = QRectF(x, y, sq_size, sq_size)

                painter.fillRect(rect, light_color if (r + c) % 2 == 0 else dark_color)

                piece_code = self._get_piece_at_square(sq_index)
                if piece_code and piece_code in self.svg_cache:
                    margin = sq_size * 0.08
                    piece_rect = QRectF(x + margin, y + margin, sq_size - 2 * margin, sq_size - 2 * margin)
                    self.svg_cache[piece_code].render(painter, piece_rect)


class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Fairy Chess")
        self.resize(940, 680)
        self.engine = EngineAdapter()
        self._init_ui()
        self._load_setup(0)

    def _init_ui(self):
        central = QWidget()
        self.setCentralWidget(central)
        layout = QHBoxLayout(central)

        self.board_widget = ChessBoardWidget()
        layout.addWidget(self.board_widget, stretch=3)

        side_panel = QVBoxLayout()

        side_panel.addWidget(QLabel("Game Mode:"))
        self.mode_selector = QComboBox()
        self.mode_selector.addItems(["Player vs Player (Hotseat)", "Player vs Computer (Minimax AI)"])
        side_panel.addWidget(self.mode_selector)

        side_panel.addWidget(QLabel("Starting Setup:"))
        self.setup_selector = QComboBox()
        self.setup_selector.addItems(["Standard Setup", "Fairy Setup"])
        self.setup_selector.currentIndexChanged.connect(self._load_setup)
        side_panel.addWidget(self.setup_selector)

        side_panel.addWidget(QLabel("Move History:"))
        self.history_list = QListWidget()
        side_panel.addWidget(self.history_list)

        btn_layout = QHBoxLayout()
        self.undo_btn = QPushButton("Undo")
        self.redo_btn = QPushButton("Redo")
        self.undo_btn.setEnabled(False)
        self.redo_btn.setEnabled(False)
        btn_layout.addWidget(self.undo_btn)
        btn_layout.addWidget(self.redo_btn)
        side_panel.addLayout(btn_layout)

        layout.addLayout(side_panel, stretch=1)

    def _load_setup(self, preset_index: int):
        resp = self.engine.send_command({"cmd": "init", "preset": preset_index})
        bitboards = resp.get("bitboards", {})
        self.board_widget.set_bitboards(bitboards)
        self.history_list.clear()


if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = MainWindow()
    window.show()
    sys.exit(app.exec())