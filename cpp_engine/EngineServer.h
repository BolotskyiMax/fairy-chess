#pragma once
// Protocol layer: reads one JSON request per line from `in`, writes one JSON answer per line to `out`.
// Commands are kept in a handler map, so a new command = one registerHandler() call (Open/Closed).

#include "Game.h"
#include "Json.h"

#include <functional>
#include <iosfwd>
#include <map>
#include <memory>
#include <string>

class StateSerializer {
public:
    // Full game state for the GUI. `extra` is appended as additional fields, e.g. "\"info\":{...}".
    static std::string serialize(const Game& game, const std::string& extra = "");
    static std::string error(const std::string& message);

private:
    static std::string pieceJson(const Piece& piece);
    static std::string moveJson(const Move& move);
};

class EngineServer {
public:
    EngineServer(std::istream& in, std::ostream& out, std::shared_ptr<const PieceFactory> factory);

    void run();
    std::string handle(const std::string& line);  // public so tests can call it directly

private:
    using Handler = std::function<std::string(const JsonObject&)>;

    void registerHandler(const std::string& command, Handler handler);
    void registerDefaultHandlers();

    std::string onMove(const JsonObject& request);

    std::istream& in_;
    std::ostream& out_;
    Game game_;
    std::map<std::string, Handler> handlers_;
    bool running_ = true;
};
