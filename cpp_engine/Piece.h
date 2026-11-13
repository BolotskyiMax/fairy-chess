#pragma once
// Movement is polymorphic: the board never asks "what type is this piece?".

#include "Types.h"

#include <memory>
#include <string>
#include <vector>

class Board;

class Piece {
public:
    Piece(Color color, int id);
    virtual ~Piece() = default;

    Color color() const { return color_; }
    int id() const { return id_; }
    int moveCount() const { return moveCount_; }
    const std::vector<int>& pathHistory() const { return path_; }

    void setStartSquare(int sq);
    void recordMove(int to);

    std::string code() const;

    virtual char symbol() const = 0;    // 'P', 'N', 'B', 'R', 'Q', 'K'
    virtual std::string name() const = 0;
    virtual int value() const = 0;      // capture points (Pawn 1, Knight 3, Bishop 4, Rook 5, Queen 9)
    virtual std::string sanLetter() const;          // letter used in move notation
    virtual bool isRoyal() const { return false; }  // losing it = losing the game
    virtual bool resetsFiftyMoveClock() const { return false; }

    // Prototype: deep copy that keeps all per-piece state.
    virtual std::unique_ptr<Piece> clone() const = 0;

    // Pseudo-legal moves (may still leave the own king in check - Board filters them).
    virtual void generateMoves(const Board& board, int from, std::vector<Move>& out) const = 0;

    // Squares this piece attacks; used for check detection.
    virtual Bitboard attacks(const Board& board, int from) const = 0;

protected:
    // Adds a move to `to` if it is empty or holds an enemy. Returns true if the square was empty.
    bool tryAddStep(const Board& board, int from, int to, std::vector<Move>& out) const;

private:
    Color color_;
    int id_;
    int moveCount_ = 0;
    std::vector<int> path_;
};

// Template Method: sliding pieces share the algorithm, subclasses only give directions.
class SlidingPiece : public Piece {
public:
    using Piece::Piece;
    void generateMoves(const Board& board, int from, std::vector<Move>& out) const override;
    Bitboard attacks(const Board& board, int from) const override;

protected:
    virtual const std::vector<Direction>& directions() const = 0;
};

// Template Method: pieces that jump to fixed offsets (Knight, King).
class LeaperPiece : public Piece {
public:
    using Piece::Piece;
    void generateMoves(const Board& board, int from, std::vector<Move>& out) const override;
    Bitboard attacks(const Board& board, int from) const override;

protected:
    virtual const std::vector<Direction>& offsets() const = 0;
};

class Pawn final : public Piece {
public:
    using Piece::Piece;
    char symbol() const override { return 'P'; }
    std::string name() const override { return "Pawn"; }
    int value() const override { return 1; }
    std::string sanLetter() const override { return ""; }
    bool resetsFiftyMoveClock() const override { return true; }
    std::unique_ptr<Piece> clone() const override { return std::make_unique<Pawn>(*this); }

    void generateMoves(const Board& board, int from, std::vector<Move>& out) const override;
    Bitboard attacks(const Board& board, int from) const override;

private:
    int forward() const;
    int startRow() const;
    int promotionRow() const;
    void addAdvance(int from, int to, int captureSquare, std::vector<Move>& out) const;
};

class Knight final : public LeaperPiece {
public:
    using LeaperPiece::LeaperPiece;
    char symbol() const override { return 'N'; }
    std::string name() const override { return "Knight"; }
    int value() const override { return 3; }
    std::unique_ptr<Piece> clone() const override { return std::make_unique<Knight>(*this); }

protected:
    const std::vector<Direction>& offsets() const override;
};

class Bishop final : public SlidingPiece {
public:
    using SlidingPiece::SlidingPiece;
    char symbol() const override { return 'B'; }
    std::string name() const override { return "Bishop"; }
    int value() const override { return 4; }
    std::unique_ptr<Piece> clone() const override { return std::make_unique<Bishop>(*this); }

protected:
    const std::vector<Direction>& directions() const override;
};

class Rook final : public SlidingPiece {
public:
    using SlidingPiece::SlidingPiece;
    char symbol() const override { return 'R'; }
    std::string name() const override { return "Rook"; }
    int value() const override { return 5; }
    std::unique_ptr<Piece> clone() const override { return std::make_unique<Rook>(*this); }

protected:
    const std::vector<Direction>& directions() const override;
};

class Queen final : public SlidingPiece {
public:
    using SlidingPiece::SlidingPiece;
    char symbol() const override { return 'Q'; }
    std::string name() const override { return "Queen"; }
    int value() const override { return 9; }
    std::unique_ptr<Piece> clone() const override { return std::make_unique<Queen>(*this); }

protected:
    const std::vector<Direction>& directions() const override;
};

class King final : public LeaperPiece {
public:
    using LeaperPiece::LeaperPiece;
    char symbol() const override { return 'K'; }
    std::string name() const override { return "King"; }
    int value() const override { return 0; }
    bool isRoyal() const override { return true; }
    std::unique_ptr<Piece> clone() const override { return std::make_unique<King>(*this); }

    // Normal king steps + castling.
    void generateMoves(const Board& board, int from, std::vector<Move>& out) const override;

protected:
    const std::vector<Direction>& offsets() const override;

private:
    void tryCastle(const Board& board, int from, bool kingSide, std::vector<Move>& out) const;
};
