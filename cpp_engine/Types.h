#pragma once

#include <cstdint>
#include <string>

using Bitboard = std::uint64_t;

enum class Color { White = 0, Black = 1 };

inline Color opposite(Color c) { return c == Color::White ? Color::Black : Color::White; }
inline int colorIndex(Color c) { return static_cast<int>(c); }
inline const char* colorName(Color c) { return c == Color::White ? "white" : "black"; }

// Squares are numbered 0..63, row 0 = rank 8 (Black's back rank), col 0 = file a.
// So a8 = 0, h8 = 7, a1 = 56, h1 = 63.
namespace Square {
    constexpr int None = -1;

    inline int make(int row, int col) { return row * 8 + col; }
    inline int row(int sq) { return sq / 8; }
    inline int col(int sq) { return sq % 8; }
    inline bool inside(int row, int col) { return row >= 0 && row < 8 && col >= 0 && col < 8; }
    inline Bitboard bit(int sq) { return 1ULL << sq; }

    inline std::string name(int sq) {
        std::string s;
        s += static_cast<char>('a' + col(sq));
        s += static_cast<char>('8' - row(sq));
        return s;
    }

    // "e4" -> square index, or None if the text is not a square.
    inline int parse(const std::string& text) {
        if (text.size() != 2) return None;
        const int c = text[0] - 'a';
        const int r = '8' - text[1];
        return inside(r, c) ? make(r, c) : None;
    }
}

struct Direction {
    int dRow;
    int dCol;
};

// A fully described move. Board::applyMove() executes it without knowing
// which piece produced it, so special moves are encoded as data.
struct Move {
    int from = Square::None;
    int to = Square::None;
    int captureSquare = Square::None;   // square of the captured piece (differs from `to` for en passant)
    int rookFrom = Square::None;        // castling only
    int rookTo = Square::None;          // castling only
    int enPassantTarget = Square::None; // square a pawn double push jumped over
    char promotion = 0;                 // 'Q', 'R', 'B', 'N' or 0

    bool isCapture() const { return captureSquare != Square::None; }
    bool isCastling() const { return rookFrom != Square::None; }

    bool matches(int f, int t, char promo) const {
        return from == f && to == t && promotion == promo;
    }
};
