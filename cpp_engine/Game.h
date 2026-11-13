#pragma once
// Game = board + rules of the match (status, notation, repetition) + Undo/Redo.
// Undo/Redo uses Command (every move is a MoveCommand) + Memento (a full snapshot
// of the game taken before and after the move).

#include "Board.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

enum class GameStatus { Playing, Check, Checkmate, Stalemate, DrawRepetition, DrawFiftyMoves };

const char* statusName(GameStatus status);

class Game;

// Memento: opaque snapshot. Only Game can read or create it.
class GameMemento {
    friend class Game;

private:
    GameMemento() = default;

    std::unique_ptr<Board> board_;
    std::vector<Move> moves_;
    std::vector<std::string> san_;
    std::vector<std::string> captured_[2];
    std::map<std::string, int> repetitions_;
};

// Command
class GameCommand {
public:
    virtual ~GameCommand() = default;
    virtual void execute(Game& game) = 0;
    virtual void undo(Game& game) = 0;
};

class MoveCommand final : public GameCommand {
public:
    explicit MoveCommand(const Move& move) : move_(move) {}
    void execute(Game& game) override;  // first time: plays the move; on redo: restores the "after" snapshot
    void undo(Game& game) override;

private:
    Move move_;
    std::unique_ptr<GameMemento> before_;
    std::unique_ptr<GameMemento> after_;
};

class CommandHistory {
public:
    void execute(std::unique_ptr<GameCommand> command, Game& game);
    bool undo(Game& game);
    bool redo(Game& game);
    bool canUndo() const { return !undo_.empty(); }
    bool canRedo() const { return !redo_.empty(); }
    void clear();

private:
    std::vector<std::unique_ptr<GameCommand>> undo_;
    std::vector<std::unique_ptr<GameCommand>> redo_;
};

class Game {
public:
    explicit Game(std::shared_ptr<const PieceFactory> factory);

    void newGame();
    void loadFen(const std::string& fen);

    // Validates the move against the legal move list; returns false and fills `error` if illegal.
    bool tryMove(int from, int to, char promotion, std::string& error);
    void playMove(const Move& move);   // the move must be legal

    bool undo() { return history_.undo(*this); }
    bool redo() { return history_.redo(*this); }
    bool canUndo() const { return history_.canUndo(); }
    bool canRedo() const { return history_.canRedo(); }

    const Board& board() const { return *board_; }
    GameStatus status() const { return status_; }
    bool isOver() const;
    std::string result() const;        // "1-0", "0-1", "1/2-1/2" or "" while playing
    const std::vector<Move>& legalMoves() const { return legal_; }
    const std::vector<Move>& moves() const { return moves_; }
    const std::vector<std::string>& sanHistory() const { return san_; }
    const std::vector<std::string>& capturedBy(Color c) const { return captured_[colorIndex(c)]; }

    // Originator API for the Memento/Command classes.
    std::unique_ptr<GameMemento> saveState() const;
    void restoreState(const GameMemento& memento);
    void performMove(const Move& move);

private:
    void resetHistory();
    void refresh();
    std::string sanWithoutSuffix(const Move& move) const;

    std::shared_ptr<const PieceFactory> factory_;
    std::unique_ptr<Board> board_;
    std::vector<Move> legal_;
    GameStatus status_ = GameStatus::Playing;
    std::vector<Move> moves_;
    std::vector<std::string> san_;
    std::vector<std::string> captured_[2];
    std::map<std::string, int> repetitions_;
    CommandHistory history_;
};
