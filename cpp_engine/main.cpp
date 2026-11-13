// Entry point of chess_engine.exe.
// Here (and only here) the concrete classes are chosen and wired together;
// everything else depends on abstractions (e.g. Board only knows PieceFactory).

#include "EngineServer.h"

#include <iostream>

int main() {
    std::ios::sync_with_stdio(false);

    auto factory = PieceFactory::createStandard();

    EngineServer server(std::cin, std::cout, factory);
    server.run();
    return 0;
}
