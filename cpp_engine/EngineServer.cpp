#include "EngineServer.h"

#include <cctype>
#include <iostream>
#include <sstream>
#include <stdexcept>

// ---------------------------------------------------------------- StateSerializer

std::string StateSerializer::pieceJson(const Piece& piece) {
    std::ostringstream o;
    o << "{\"code\":" << jsonQuote(piece.code())
      << ",\"id\":" << piece.id()
      << ",\"name\":" << jsonQuote(piece.name())
      << ",\"color\":" << jsonQuote(colorName(piece.color()))
      << ",\"value\":" << piece.value()
      << ",\"moves\":" << piece.moveCount()
      << ",\"path\":[";
    const auto& path = piece.pathHistory();
    for (size_t i = 0; i < path.size(); ++i) {
        if (i) o << ',';
        o << jsonQuote(Square::name(path[i]));
    }
    o << "]}";
    return o.str();
}

std::string StateSerializer::moveJson(const Move& move) {
    std::ostringstream o;
    o << "{\"from\":" << move.from << ",\"to\":" << move.to << ",\"promo\":"
      << jsonQuote(move.promotion ? std::string(1, move.promotion) : "") << '}';
    return o.str();
}

std::string StateSerializer::serialize(const Game& game, const std::string& extra) {
    const Board& board = game.board();
    const Color turn = board.sideToMove();
    std::ostringstream o;

    o << "{\"status\":\"ok\""
      << ",\"state\":" << jsonQuote(statusName(game.status()))
      << ",\"result\":" << jsonQuote(game.result())
      << ",\"turn\":" << jsonQuote(colorName(turn))
      << ",\"fullmove\":" << board.fullmoveNumber()
      << ",\"check_square\":" << (board.inCheck(turn) ? board.kingSquare(turn) : Square::None);

    o << ",\"board\":[";
    for (int sq = 0; sq < 64; ++sq) {
        if (sq) o << ',';
        const Piece* piece = board.pieceAt(sq);
        o << (piece ? pieceJson(*piece) : "null");
    }
    o << ']';

    o << ",\"legal_moves\":[";
    const auto& legal = game.legalMoves();
    for (size_t i = 0; i < legal.size(); ++i) {
        if (i) o << ',';
        o << moveJson(legal[i]);
    }
    o << ']';

    o << ",\"history\":[";
    const auto& san = game.sanHistory();
    for (size_t i = 0; i < san.size(); ++i) {
        if (i) o << ',';
        o << jsonQuote(san[i]);
    }
    o << ']';

    o << ",\"last_move\":" << (game.moves().empty() ? "null" : moveJson(game.moves().back()));

    o << ",\"captured\":{";
    for (Color c : {Color::White, Color::Black}) {
        if (c == Color::Black) o << ',';
        o << jsonQuote(colorName(c)) << ":[";
        const auto& list = game.capturedBy(c);
        for (size_t i = 0; i < list.size(); ++i) {
            if (i) o << ',';
            o << jsonQuote(list[i]);
        }
        o << ']';
    }
    o << '}';

    o << ",\"material\":{\"white\":" << board.material(Color::White)
      << ",\"black\":" << board.material(Color::Black) << '}';
    o << ",\"can_undo\":" << (game.canUndo() ? "true" : "false")
      << ",\"can_redo\":" << (game.canRedo() ? "true" : "false");

    if (!extra.empty()) o << ',' << extra;
    o << '}';
    return o.str();
}

std::string StateSerializer::error(const std::string& message) {
    return "{\"status\":\"error\",\"message\":" + jsonQuote(message) + "}";
}

// ---------------------------------------------------------------- EngineServer

EngineServer::EngineServer(std::istream& in, std::ostream& out, std::shared_ptr<const PieceFactory> factory)
    : in_(in), out_(out), game_(std::move(factory)) {
    registerDefaultHandlers();
}

void EngineServer::registerHandler(const std::string& command, Handler handler) {
    handlers_[command] = std::move(handler);
}

void EngineServer::registerDefaultHandlers() {
    registerHandler("state", [this](const JsonObject&) {
        return StateSerializer::serialize(game_);
    });
    registerHandler("new_game", [this](const JsonObject&) {
        game_.newGame();
        return StateSerializer::serialize(game_);
    });
    registerHandler("load_fen", [this](const JsonObject& r) {
        game_.loadFen(r.getString("fen"));
        return StateSerializer::serialize(game_);
    });
    registerHandler("move", [this](const JsonObject& r) { return onMove(r); });
    registerHandler("undo", [this](const JsonObject&) {
        return game_.undo() ? StateSerializer::serialize(game_) : StateSerializer::error("Nothing to undo");
    });
    registerHandler("redo", [this](const JsonObject&) {
        return game_.redo() ? StateSerializer::serialize(game_) : StateSerializer::error("Nothing to redo");
    });
    registerHandler("quit", [this](const JsonObject&) {
        running_ = false;
        return std::string("{\"status\":\"ok\",\"state\":\"quit\"}");
    });
}

std::string EngineServer::onMove(const JsonObject& request) {
    const long long from = request.getInt("from", -1);
    const long long to = request.getInt("to", -1);
    if (from < 0 || from > 63 || to < 0 || to > 63) {
        return StateSerializer::error("'from' and 'to' must be squares 0..63");
    }
    const std::string promo = request.getString("promo");
    const char promotion = promo.empty() ? 0 : static_cast<char>(std::toupper(static_cast<unsigned char>(promo[0])));

    std::string error;
    if (!game_.tryMove(static_cast<int>(from), static_cast<int>(to), promotion, error)) {
        return StateSerializer::error(error);
    }
    return StateSerializer::serialize(game_);
}

std::string EngineServer::handle(const std::string& line) {
    try {
        const JsonObject request = JsonObject::parse(line);
        const std::string command = request.getString("cmd");
        const auto it = handlers_.find(command);
        if (it == handlers_.end()) return StateSerializer::error("Unknown command: '" + command + "'");
        return it->second(request);
    } catch (const std::exception& e) {
        return StateSerializer::error(e.what());
    }
}

void EngineServer::run() {
    std::string line;
    while (running_ && std::getline(in_, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        out_ << handle(line) << std::endl;  // std::endl flushes - Python waits for this line
    }
}
