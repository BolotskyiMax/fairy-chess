#include "PieceFactory.h"

#include <stdexcept>

void PieceFactory::registerCreator(char symbol, std::unique_ptr<PieceCreator> creator) {
    creators_[symbol] = std::move(creator);
}

bool PieceFactory::knows(char symbol) const {
    return creators_.count(symbol) != 0;
}

std::unique_ptr<Piece> PieceFactory::create(char symbol, Color color, int id) const {
    const auto it = creators_.find(symbol);
    if (it == creators_.end()) {
        throw std::invalid_argument(std::string("Unknown piece symbol: ") + symbol);
    }
    return it->second->create(color, id);
}

std::shared_ptr<PieceFactory> PieceFactory::createStandard() {
    auto factory = std::make_shared<PieceFactory>();
    factory->registerCreator('P', std::make_unique<ConcretePieceCreator<Pawn>>());
    factory->registerCreator('N', std::make_unique<ConcretePieceCreator<Knight>>());
    factory->registerCreator('B', std::make_unique<ConcretePieceCreator<Bishop>>());
    factory->registerCreator('R', std::make_unique<ConcretePieceCreator<Rook>>());
    factory->registerCreator('Q', std::make_unique<ConcretePieceCreator<Queen>>());
    factory->registerCreator('K', std::make_unique<ConcretePieceCreator<King>>());
    return factory;
}
