#include "Board.h"

#include <cctype>
#include <sstream>
#include <stdexcept>

namespace {
    const char* const kStandardFen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
}

Board::Board(std::shared_ptr<const PieceFactory> factory) : factory_(std::move(factory)) {}

Board::Board(const Board& other)
    : factory_(other.factory_),
      side_(other.side_),
      enPassant_(other.enPassant_),
      halfmove_(other.halfmove_),
      fullmove_(other.fullmove_),
      nextId_(other.nextId_) {
    for (int sq = 0; sq < 64; ++sq) {
        if (other.squares_[sq]) squares_[sq] = other.squares_[sq]->clone();
    }
    occupied_[0] = other.occupied_[0];
    occupied_[1] = other.occupied_[1];
    for (int c = 0; c < 2; ++c) {
        for (int s = 0; s < 2; ++s) castling_[c][s] = other.castling_[c][s];
    }
}

std::unique_ptr<Board> Board::clone() const {
    return std::make_unique<Board>(*this);
}

// ---------------------------------------------------------------- setup

void Board::clear() {
    for (auto& sq : squares_) sq.reset();
    occupied_[0] = occupied_[1] = 0;
    side_ = Color::White;
    for (auto& rights : castling_) rights[0] = rights[1] = false;
    enPassant_ = Square::None;
    halfmove_ = 0;
    fullmove_ = 1;
    nextId_ = 1;
}

void Board::setupStandard() {
    loadFen(kStandardFen);
}

void Board::loadFen(const std::string& fen) {
    std::istringstream in(fen);
    std::string placement, side, castling = "-", enPassant = "-";
    int halfmove = 0, fullmove = 1;
    in >> placement >> side >> castling >> enPassant >> halfmove >> fullmove;
    if (placement.empty() || side.empty()) throw std::invalid_argument("FEN: missing fields");

    clear();
    int row = 0, col = 0;
    for (char ch : placement) {
        if (ch == '/') {
            if (col != 8) throw std::invalid_argument("FEN: bad row length");
            ++row;
            col = 0;
        } else if (std::isdigit(static_cast<unsigned char>(ch))) {
            col += ch - '0';
        } else {
            const char symbol = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
            if (!Square::inside(row, col) || !factory_->knows(symbol)) {
                throw std::invalid_argument(std::string("FEN: bad piece ") + ch);
            }
            const Color color = std::isupper(static_cast<unsigned char>(ch)) ? Color::White : Color::Black;
            placeNew(symbol, color, Square::make(row, col));
            ++col;
        }
    }
    if (row != 7 || col != 8) throw std::invalid_argument("FEN: bad placement");

    if (side == "w") side_ = Color::White;
    else if (side == "b") side_ = Color::Black;
    else throw std::invalid_argument("FEN: bad side to move");

    for (char ch : castling) {
        if (ch == 'K') castling_[0][0] = true;
        if (ch == 'Q') castling_[0][1] = true;
        if (ch == 'k') castling_[1][0] = true;
        if (ch == 'q') castling_[1][1] = true;
    }
    enPassant_ = enPassant == "-" ? Square::None : Square::parse(enPassant);
    halfmove_ = halfmove;
    fullmove_ = fullmove > 0 ? fullmove : 1;
}

void Board::placeNew(char symbol, Color color, int sq) {
    auto piece = factory_->create(symbol, color, nextId_++);
    piece->setStartSquare(sq);
    put(sq, std::move(piece));
}

// ---------------------------------------------------------------- queries

int Board::kingSquare(Color c) const {
    for (int sq = 0; sq < 64; ++sq) {
        const Piece* p = squares_[sq].get();
        if (p != nullptr && p->color() == c && p->isRoyal()) return sq;
    }
    return Square::None;
}

Bitboard Board::attackedBy(Color c) const {
    Bitboard result = 0;
    for (int sq = 0; sq < 64; ++sq) {
        const Piece* p = squares_[sq].get();
        if (p != nullptr && p->color() == c) result |= p->attacks(*this, sq);
    }
    return result;
}

bool Board::isSquareAttacked(int sq, Color by) const {
    const Bitboard target = Square::bit(sq);
    for (int from = 0; from < 64; ++from) {
        const Piece* p = squares_[from].get();
        if (p != nullptr && p->color() == by && (p->attacks(*this, from) & target)) return true;
    }
    return false;
}

bool Board::inCheck(Color c) const {
    const int king = kingSquare(c);
    return king != Square::None && isSquareAttacked(king, opposite(c));
}

int Board::material(Color c) const {
    int total = 0;
    for (const auto& p : squares_) {
        if (p && p->color() == c) total += p->value();
    }
    return total;
}

std::vector<Move> Board::pseudoLegalMoves() const {
    std::vector<Move> moves;
    moves.reserve(64);
    for (int sq = 0; sq < 64; ++sq) {
        const Piece* p = squares_[sq].get();
        if (p != nullptr && p->color() == side_) p->generateMoves(*this, sq, moves);
    }
    return moves;
}

std::vector<Move> Board::legalMoves() const {
    std::vector<Move> legal;
    for (const Move& move : pseudoLegalMoves()) {
        Board copy(*this);
        copy.applyMove(move);
        if (!copy.inCheck(side_)) legal.push_back(move);
    }
    return legal;
}

std::string Board::positionKey() const {
    std::string key;
    key.reserve(72);
    for (const auto& p : squares_) {
        if (!p) {
            key += '.';
        } else {
            const char s = p->symbol();
            key += p->color() == Color::White ? s : static_cast<char>(std::tolower(static_cast<unsigned char>(s)));
        }
    }
    key += side_ == Color::White ? 'w' : 'b';
    for (const auto& rights : castling_) {
        key += rights[0] ? '1' : '0';
        key += rights[1] ? '1' : '0';
    }
    key += std::to_string(enPassant_);
    return key;
}

// ---------------------------------------------------------------- mutation

void Board::put(int sq, std::unique_ptr<Piece> piece) {
    occupied_[colorIndex(piece->color())] |= Square::bit(sq);
    squares_[sq] = std::move(piece);
}

std::unique_ptr<Piece> Board::take(int sq) {
    std::unique_ptr<Piece> piece = std::move(squares_[sq]);
    if (piece) occupied_[colorIndex(piece->color())] &= ~Square::bit(sq);
    return piece;
}

void Board::updateCastlingRights(int sq) {
    // Rights disappear as soon as anything moves from or to a king/rook home square.
    switch (sq) {
        case 60: castling_[0][0] = castling_[0][1] = false; break;  // e1
        case 63: castling_[0][0] = false; break;                     // h1
        case 56: castling_[0][1] = false; break;                     // a1
        case 4:  castling_[1][0] = castling_[1][1] = false; break;   // e8
        case 7:  castling_[1][0] = false; break;                     // h8
        case 0:  castling_[1][1] = false; break;                     // a8
        default: break;
    }
}

std::unique_ptr<Piece> Board::applyMove(const Move& move) {
    std::unique_ptr<Piece> captured;
    if (move.isCapture()) captured = take(move.captureSquare);

    std::unique_ptr<Piece> mover = take(move.from);
    const bool resetClock = captured != nullptr || mover->resetsFiftyMoveClock();
    mover->recordMove(move.to);

    if (move.promotion != 0) {
        // The pawn becomes a brand-new ("clean") piece of the chosen type.
        auto promoted = factory_->create(move.promotion, mover->color(), mover->id());
        promoted->setStartSquare(move.to);
        mover = std::move(promoted);
    }
    put(move.to, std::move(mover));

    if (move.isCastling()) {
        std::unique_ptr<Piece> rook = take(move.rookFrom);
        rook->recordMove(move.rookTo);  // castling counts as a rook move too
        put(move.rookTo, std::move(rook));
        updateCastlingRights(move.rookFrom);
    }

    updateCastlingRights(move.from);
    updateCastlingRights(move.to);
    enPassant_ = move.enPassantTarget;
    halfmove_ = resetClock ? 0 : halfmove_ + 1;
    if (side_ == Color::Black) ++fullmove_;
    side_ = opposite(side_);
    return captured;
}
