// Engine self-tests. Build and run with:  run.bat test
// Perft counts the number of positions reachable in N moves and compares them with
// well-known reference values - any bug in move generation changes these numbers.

#include "../EngineServer.h"
#include "../Game.h"

#include <cctype>
#include <iostream>
#include <sstream>
#include <string>

namespace {
    int failures = 0;

    void check(bool condition, const std::string& name) {
        std::cout << (condition ? "[PASS] " : "[FAIL] ") << name << '\n';
        if (!condition) ++failures;
    }

    long long perft(const Board& board, int depth) {
        if (depth == 0) return 1;
        long long nodes = 0;
        for (const Move& move : board.legalMoves()) {
            Board child(board);
            child.applyMove(move);
            nodes += perft(child, depth - 1);
        }
        return nodes;
    }

    // Plays moves given as "e2e4", "e7e8q", ...
    bool play(Game& game, const std::string& moves) {
        std::istringstream in(moves);
        std::string m;
        while (in >> m) {
            const int from = Square::parse(m.substr(0, 2));
            const int to = Square::parse(m.substr(2, 2));
            const char promo = m.size() > 4 ? static_cast<char>(std::toupper(m[4])) : 0;
            std::string error;
            if (!game.tryMove(from, to, promo, error)) {
                std::cout << "       move " << m << " rejected: " << error << '\n';
                return false;
            }
        }
        return true;
    }
}

int main() {
    auto factory = PieceFactory::createStandard();

    // --- Perft (reference values from the Chess Programming Wiki)
    struct PerftCase { const char* name; const char* fen; int depth; long long expected; };
    const PerftCase cases[] = {
        {"start position", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 3, 8902},
        {"kiwipete (castling, en passant)", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 2, 2039},
        {"position 3 (pins, en passant)", "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 4, 43238},
        {"position 4 (promotions)", "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 3, 9467},
        {"position 5", "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", 3, 62379},
    };
    for (const auto& c : cases) {
        Board board(factory);
        board.loadFen(c.fen);
        const long long nodes = perft(board, c.depth);
        check(nodes == c.expected, std::string("perft ") + c.name + " = " + std::to_string(nodes));
    }

    // --- Game rules
    {
        Game game(factory);
        play(game, "f2f3 e7e5 g2g4 d8h4");
        check(game.status() == GameStatus::Checkmate && game.result() == "0-1", "fool's mate is checkmate");
        check(game.sanHistory().back() == "Qh4#", "mate is written as Qh4#");
    }
    {
        Game game(factory);
        game.loadFen("7k/5Q2/6K1/8/8/8/8/8 b - - 0 1");
        check(game.status() == GameStatus::Stalemate, "stalemate detected");
    }
    {
        Game game(factory);
        game.loadFen("4k3/8/8/8/8/8/5r2/R3K2R w KQ - 0 1");
        std::string error;
        check(!game.tryMove(60, 62, 0, error), "cannot castle through an attacked square");
        check(game.tryMove(60, 58, 0, error) && game.sanHistory().back() == "O-O-O", "queen-side castling works");
        check(game.board().pieceAt(59) && game.board().pieceAt(59)->symbol() == 'R', "rook moved to d1");
    }
    {
        Game game(factory);
        play(game, "e2e4 a7a6 e4e5 d7d5");
        std::string error;
        check(game.tryMove(Square::parse("e5"), Square::parse("d6"), 0, error), "en passant is legal");
        check(game.board().isEmpty(Square::parse("d5")), "en passant removes the pawn");
        check(game.sanHistory().back() == "exd6", "en passant notation exd6");
    }
    {
        Game game(factory);
        game.loadFen("8/P6k/8/8/8/8/8/K7 w - - 0 1");
        check(play(game, "a7a8n"), "under-promotion to knight");
        check(game.board().pieceAt(0)->symbol() == 'N' && game.sanHistory().back() == "a8=N", "promoted piece is a knight");
    }
    {
        Game game(factory);
        play(game, "g1f3 g8f6 f3g1 f6g8 g1f3 g8f6 f3g1 f6g8");
        check(game.status() == GameStatus::DrawRepetition, "threefold repetition");
    }
    {
        Game game(factory);
        play(game, "e2e4 e7e5 g1f3");
        const std::string afterThree = game.board().positionKey();
        check(game.undo() && game.undo() && game.sanHistory().size() == 1, "undo twice");
        check(game.redo() && game.redo() && game.board().positionKey() == afterThree, "redo restores the position");
        play(game, "b8c6");
        check(!game.canRedo(), "a new move clears the redo stack");
        check(game.board().pieceAt(Square::parse("f3"))->moveCount() == 1, "piece move counter survives undo/redo");
    }

    // --- Protocol
    {
        std::istringstream in;
        std::ostringstream out;
        EngineServer server(in, out, factory);
        check(server.handle("{\"cmd\":\"new_game\"}").find("\"status\":\"ok\"") != std::string::npos, "protocol: new_game");
        check(server.handle("{\"cmd\":\"move\",\"from\":52,\"to\":36}").find("\"history\":[\"e4\"]") != std::string::npos, "protocol: move e2-e4");
        check(server.handle("{\"cmd\":\"move\",\"from\":52,\"to\":28}").find("Illegal move") != std::string::npos, "protocol: illegal move rejected");
        check(server.handle("not json").find("\"status\":\"error\"") != std::string::npos, "protocol: bad JSON gives an error");
        check(server.handle("{\"cmd\":\"fly\"}").find("Unknown command") != std::string::npos, "protocol: unknown command");
    }

    std::cout << (failures == 0 ? "\nAll tests passed.\n" : "\nSome tests FAILED.\n");
    return failures == 0 ? 0 : 1;
}
