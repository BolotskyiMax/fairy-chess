#include "Piece.h"

#include "Board.h"

// ---------------------------------------------------------------- Piece

Piece::Piece(Color color, int id) : color_(color), id_(id) {}

void Piece::setStartSquare(int sq) {
    path_.assign(1, sq);
}

void Piece::recordMove(int to) {
    ++moveCount_;
    path_.push_back(to);
}

std::string Piece::code() const {
    std::string result(1, color_ == Color::White ? 'w' : 'b');
    result += symbol();
    return result;
}

std::string Piece::sanLetter() const {
    return std::string(1, symbol());
}

bool Piece::tryAddStep(const Board& board, int from, int to, std::vector<Move>& out) const {
    Move move;
    move.from = from;
    move.to = to;
    if (board.isEmpty(to)) {
        out.push_back(move);
        return true;
    }
    if (board.isEnemy(to, color_)) {
        move.captureSquare = to;
        out.push_back(move);
    }
    return false;
}

// ---------------------------------------------------------------- SlidingPiece

void SlidingPiece::generateMoves(const Board& board, int from, std::vector<Move>& out) const {
    for (const Direction& d : directions()) {
        int r = Square::row(from) + d.dRow;
        int c = Square::col(from) + d.dCol;
        while (Square::inside(r, c)) {
            if (!tryAddStep(board, from, Square::make(r, c), out)) break;
            r += d.dRow;
            c += d.dCol;
        }
    }
}

Bitboard SlidingPiece::attacks(const Board& board, int from) const {
    Bitboard result = 0;
    for (const Direction& d : directions()) {
        int r = Square::row(from) + d.dRow;
        int c = Square::col(from) + d.dCol;
        while (Square::inside(r, c)) {
            const int sq = Square::make(r, c);
            result |= Square::bit(sq);
            if (!board.isEmpty(sq)) break;
            r += d.dRow;
            c += d.dCol;
        }
    }
    return result;
}

// ---------------------------------------------------------------- LeaperPiece

void LeaperPiece::generateMoves(const Board& board, int from, std::vector<Move>& out) const {
    for (const Direction& o : offsets()) {
        const int r = Square::row(from) + o.dRow;
        const int c = Square::col(from) + o.dCol;
        if (Square::inside(r, c)) tryAddStep(board, from, Square::make(r, c), out);
    }
}

Bitboard LeaperPiece::attacks(const Board&, int from) const {
    Bitboard result = 0;
    for (const Direction& o : offsets()) {
        const int r = Square::row(from) + o.dRow;
        const int c = Square::col(from) + o.dCol;
        if (Square::inside(r, c)) result |= Square::bit(Square::make(r, c));
    }
    return result;
}

// ---------------------------------------------------------------- Pawn

int Pawn::forward() const { return color() == Color::White ? -1 : 1; }
int Pawn::startRow() const { return color() == Color::White ? 6 : 1; }
int Pawn::promotionRow() const { return color() == Color::White ? 0 : 7; }

void Pawn::generateMoves(const Board& board, int from, std::vector<Move>& out) const {
    const int r = Square::row(from);
    const int c = Square::col(from);
    const int r1 = r + forward();
    if (!Square::inside(r1, c)) return;

    // Quiet pushes.
    const int one = Square::make(r1, c);
    if (board.isEmpty(one)) {
        addAdvance(from, one, Square::None, out);
        const int r2 = r + 2 * forward();
        if (r == startRow() && board.isEmpty(Square::make(r2, c))) {
            Move doublePush;
            doublePush.from = from;
            doublePush.to = Square::make(r2, c);
            doublePush.enPassantTarget = one;
            out.push_back(doublePush);
        }
    }

    // Diagonal captures, including en passant.
    for (int dc : {-1, 1}) {
        if (!Square::inside(r1, c + dc)) continue;
        const int target = Square::make(r1, c + dc);
        if (board.isEnemy(target, color())) {
            addAdvance(from, target, target, out);
        } else if (target == board.enPassantSquare()) {
            addAdvance(from, target, Square::make(r, c + dc), out);
        }
    }
}

Bitboard Pawn::attacks(const Board&, int from) const {
    Bitboard result = 0;
    const int r1 = Square::row(from) + forward();
    for (int dc : {-1, 1}) {
        const int c = Square::col(from) + dc;
        if (Square::inside(r1, c)) result |= Square::bit(Square::make(r1, c));
    }
    return result;
}

void Pawn::addAdvance(int from, int to, int captureSquare, std::vector<Move>& out) const {
    Move move;
    move.from = from;
    move.to = to;
    move.captureSquare = captureSquare;
    if (Square::row(to) != promotionRow()) {
        out.push_back(move);
        return;
    }
    for (char promo : {'Q', 'R', 'B', 'N'}) {
        move.promotion = promo;
        out.push_back(move);
    }
}

// ---------------------------------------------------------------- Directions

namespace {
    const std::vector<Direction> kDiagonal = {{-1, -1}, {-1, 1}, {1, -1}, {1, 1}};
    const std::vector<Direction> kOrthogonal = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
    const std::vector<Direction> kAllDirections = {{-1, -1}, {-1, 1}, {1, -1}, {1, 1},
                                                   {-1, 0},  {1, 0},  {0, -1}, {0, 1}};
    const std::vector<Direction> kKnightJumps = {{-2, -1}, {-2, 1}, {-1, -2}, {-1, 2},
                                                 {1, -2},  {1, 2},  {2, -1},  {2, 1}};
}

const std::vector<Direction>& Knight::offsets() const { return kKnightJumps; }
const std::vector<Direction>& Bishop::directions() const { return kDiagonal; }
const std::vector<Direction>& Rook::directions() const { return kOrthogonal; }
const std::vector<Direction>& Queen::directions() const { return kAllDirections; }
const std::vector<Direction>& King::offsets() const { return kAllDirections; }

// ---------------------------------------------------------------- King (castling)

void King::generateMoves(const Board& board, int from, std::vector<Move>& out) const {
    LeaperPiece::generateMoves(board, from, out);

    const int homeRow = color() == Color::White ? 7 : 0;
    if (from != Square::make(homeRow, 4)) return;
    if (board.isSquareAttacked(from, opposite(color()))) return;  // no castling out of check

    tryCastle(board, from, true, out);
    tryCastle(board, from, false, out);
}

void King::tryCastle(const Board& board, int from, bool kingSide, std::vector<Move>& out) const {
    if (!board.canCastle(color(), kingSide)) return;

    const int row = Square::row(from);
    const int rookCol = kingSide ? 7 : 0;
    const int rookFrom = Square::make(row, rookCol);
    const Piece* rook = board.pieceAt(rookFrom);
    if (rook == nullptr || rook->color() != color()) return;

    const int step = kingSide ? 1 : -1;
    for (int c = 4 + step; c != rookCol; c += step) {
        if (!board.isEmpty(Square::make(row, c))) return;
    }
    // The king may not pass through or land on an attacked square.
    for (int k = 1; k <= 2; ++k) {
        if (board.isSquareAttacked(Square::make(row, 4 + k * step), opposite(color()))) return;
    }

    Move castle;
    castle.from = from;
    castle.to = Square::make(row, 4 + 2 * step);
    castle.rookFrom = rookFrom;
    castle.rookTo = Square::make(row, 4 + step);
    out.push_back(castle);
}
