#pragma once
// The board stores piece OBJECTS (source of truth) and keeps occupancy bitboards
// as a fast cache for "is this square empty / whose piece is here" questions.

#include "Piece.h"
#include "PieceFactory.h"

#include <array>
#include <memory>
#include <string>
#include <vector>

class Board {
public:
    // Dependency Inversion: the board gets its factory from outside.
    explicit Board(std::shared_ptr<const PieceFactory> factory);
    Board(const Board& other);               // deep copy through Piece::clone()
    Board& operator=(const Board&) = delete;

    std::unique_ptr<Board> clone() const;    // Prototype

    // --- setup
    void clear();
    void setupStandard();
    void loadFen(const std::string& fen);    // throws std::invalid_argument

    // --- queries
    const Piece* pieceAt(int sq) const { return squares_[sq].get(); }
    bool isEmpty(int sq) const { return (occupancy() & Square::bit(sq)) == 0; }
    bool isEnemy(int sq, Color of) const { return (occupancy(opposite(of)) & Square::bit(sq)) != 0; }
    Bitboard occupancy() const { return occupied_[0] | occupied_[1]; }
    Bitboard occupancy(Color c) const { return occupied_[colorIndex(c)]; }

    Color sideToMove() const { return side_; }
    int enPassantSquare() const { return enPassant_; }
    bool canCastle(Color c, bool kingSide) const { return castling_[colorIndex(c)][kingSide ? 0 : 1]; }
    int halfmoveClock() const { return halfmove_; }
    int fullmoveNumber() const { return fullmove_; }

    int kingSquare(Color c) const;                     // Square::None if there is no king
    Bitboard attackedBy(Color c) const;
    bool isSquareAttacked(int sq, Color by) const;
    bool inCheck(Color c) const;
    int material(Color c) const;

    std::vector<Move> pseudoLegalMoves() const;
    std::vector<Move> legalMoves() const;

    // --- mutation (no legality check here; Game validates moves first)
    std::unique_ptr<Piece> applyMove(const Move& move);  // returns the captured piece, if any

    // Key used for threefold repetition: placement + side + castling + en passant.
    std::string positionKey() const;

private:
    void put(int sq, std::unique_ptr<Piece> piece);
    std::unique_ptr<Piece> take(int sq);
    void placeNew(char symbol, Color color, int sq);
    void updateCastlingRights(int sq);

    std::shared_ptr<const PieceFactory> factory_;
    std::array<std::unique_ptr<Piece>, 64> squares_;
    Bitboard occupied_[2] = {0, 0};
    Color side_ = Color::White;
    bool castling_[2][2] = {{false, false}, {false, false}};  // [color][0 = king side, 1 = queen side]
    int enPassant_ = Square::None;
    int halfmove_ = 0;
    int fullmove_ = 1;
    int nextId_ = 1;
};
