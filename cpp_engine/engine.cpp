#include <iostream>
#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <sstream>

using Bitboard = uint64_t;

enum class Color { White = 0, Black = 1 };

enum class PieceType {
    Pawn, Knight, Bishop, Rook, Queen, King,
    Chancellor,
    Archbishop,
    Nightrider
};

namespace BitboardUtils {
    inline Bitboard sqBit(int row, int col) {
        return 1ULL << (row * 8 + col);
    }
    inline bool getBit(Bitboard bb, int sq) {
        return (bb & (1ULL << sq)) != 0;
    }
    inline void setBit(Bitboard& bb, int row, int col) {
        bb |= sqBit(row, col);
    }
}

class Piece {
protected:
    Color color;
    PieceType type;

public:
    Piece(Color c, PieceType t) : color(c), type(t) {}
    virtual ~Piece() = default;

    Color getColor() const { return color; }
    PieceType getType() const { return type; }

    virtual std::string getCode() const = 0;
    virtual std::unique_ptr<Piece> clone() const = 0;
};

class ChessPiece : public Piece {
    std::string code;
public:
    ChessPiece(Color c, PieceType t, const std::string& svg_code)
        : Piece(c, t), code(svg_code) {}

    std::string getCode() const override { return code; }
    std::unique_ptr<Piece> clone() const override {
        return std::make_unique<ChessPiece>(*this);
    }
};

class BitboardBoard {
private:
    std::map<std::string, Bitboard> piece_bitboards;
    Bitboard occupied_white = 0ULL;
    Bitboard occupied_black = 0ULL;
    Bitboard occupied_all   = 0ULL;

    void placePiece(const std::string& code, Color c, int row, int col) {
        BitboardUtils::setBit(piece_bitboards[code], row, col);
        if (c == Color::White) {
            BitboardUtils::setBit(occupied_white, row, col);
        } else {
            BitboardUtils::setBit(occupied_black, row, col);
        }
        occupied_all = occupied_white | occupied_black;
    }

public:
    BitboardBoard() {
        setupPosition(0);
    }

    void clear() {
        piece_bitboards.clear();
        occupied_white = 0ULL;
        occupied_black = 0ULL;
        occupied_all   = 0ULL;
    }

    void setupPosition(int preset_id) {
        clear();
        if (preset_id == 0) {
            const std::string backRankB[8] = {"bR", "bN", "bB", "bQ", "bK", "bB", "bN", "bR"};
            const std::string backRankW[8] = {"wR", "wN", "wB", "wQ", "wK", "wB", "wN", "wR"};
            for (int c = 0; c < 8; ++c) {
                placePiece(backRankB[c], Color::Black, 0, c);
                placePiece("bP", Color::Black, 1, c);
                placePiece("wP", Color::White, 6, c);
                placePiece(backRankW[c], Color::White, 7, c);
            }
        } else {
            const std::string backRankB[8] = {"bN", "bR", "bB", "bQ", "bK", "bB", "bR", "bN"};
            const std::string backRankW[8] = {"wN", "wR", "wB", "wQ", "wK", "wB", "wR", "wN"};
            for (int c = 0; c < 8; ++c) {
                placePiece(backRankB[c], Color::Black, 0, c);
                placePiece((c == 3 || c == 4) ? "bB" : "bP", Color::Black, 1, c);
                placePiece((c == 3 || c == 4) ? "wB" : "wP", Color::White, 6, c);
                placePiece(backRankW[c], Color::White, 7, c);
            }
        }
    }

    std::string getPieceAt(int sq) const {
        if (!BitboardUtils::getBit(occupied_all, sq)) return "";
        for (const auto& kv : piece_bitboards) {
            if (BitboardUtils::getBit(kv.second, sq)) {
                return kv.first;
            }
        }
        return "";
    }

    std::string toJson() const {
        std::ostringstream oss;
        oss << "{\"status\":\"ok\",\"state\":\"playing\",";
        oss << "\"occupied_all\":\"" << occupied_all << "\",";
        oss << "\"bitboards\":{";
        bool first = true;
        for (const auto& kv : piece_bitboards) {
            if (!first) oss << ",";
            oss << "\"" << kv.first << "\":" << kv.second;
            first = false;
        }
        oss << "}}";
        return oss.str();
    }
};

int main() {
    BitboardBoard board;
    std::string line;

    while (std::getline(std::cin, line)) {
        if (line.find("\"cmd\":\"quit\"") != std::string::npos) {
            break;
        } else if (line.find("\"preset\":1") != std::string::npos) {
            board.setupPosition(1);
            std::cout << board.toJson() << std::endl;
        } else if (line.find("\"preset\":0") != std::string::npos || line.find("\"cmd\":\"init\"") != std::string::npos) {
            board.setupPosition(0);
            std::cout << board.toJson() << std::endl;
        } else {
            std::cout << board.toJson() << std::endl;
        }
    }
    return 0;
}