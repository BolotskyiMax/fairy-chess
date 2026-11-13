#pragma once
// Factory Method: each PieceCreator subclass knows how to create one piece type.
// PieceFactory keeps a registry symbol -> creator, so adding a new piece type means
// registering one more creator instead of editing a switch (Open/Closed principle).

#include "Piece.h"

#include <map>
#include <memory>

class PieceCreator {
public:
    virtual ~PieceCreator() = default;
    virtual std::unique_ptr<Piece> create(Color color, int id) const = 0;
};

template <typename T>
class ConcretePieceCreator final : public PieceCreator {
public:
    std::unique_ptr<Piece> create(Color color, int id) const override {
        return std::make_unique<T>(color, id);
    }
};

class PieceFactory {
public:
    void registerCreator(char symbol, std::unique_ptr<PieceCreator> creator);
    bool knows(char symbol) const;

    // Throws std::invalid_argument for an unknown symbol.
    std::unique_ptr<Piece> create(char symbol, Color color, int id) const;

    static std::shared_ptr<PieceFactory> createStandard();

private:
    std::map<char, std::unique_ptr<PieceCreator>> creators_;
};
