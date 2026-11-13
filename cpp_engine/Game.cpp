#include "Game.h"

const char* statusName(GameStatus status) {
    switch (status) {
        case GameStatus::Playing: return "playing";
        case GameStatus::Check: return "check";
        case GameStatus::Checkmate: return "checkmate";
        case GameStatus::Stalemate: return "stalemate";
        case GameStatus::DrawRepetition: return "draw_repetition";
        case GameStatus::DrawFiftyMoves: return "draw_fifty_moves";
    }
    return "playing";
}

// ---------------------------------------------------------------- MoveCommand

void MoveCommand::execute(Game& game) {
    if (after_) {               // redo
        game.restoreState(*after_);
        return;
    }
    before_ = game.saveState();
    game.performMove(move_);
    after_ = game.saveState();  // later this also freezes dice rolls, so redo repeats them exactly
}

void MoveCommand::undo(Game& game) {
    game.restoreState(*before_);
}

// ---------------------------------------------------------------- CommandHistory

void CommandHistory::execute(std::unique_ptr<GameCommand> command, Game& game) {
    command->execute(game);
    undo_.push_back(std::move(command));
    redo_.clear();
}

bool CommandHistory::undo(Game& game) {
    if (undo_.empty()) return false;
    std::unique_ptr<GameCommand> command = std::move(undo_.back());
    undo_.pop_back();
    command->undo(game);
    redo_.push_back(std::move(command));
    return true;
}

bool CommandHistory::redo(Game& game) {
    if (redo_.empty()) return false;
    std::unique_ptr<GameCommand> command = std::move(redo_.back());
    redo_.pop_back();
    command->execute(game);
    undo_.push_back(std::move(command));
    return true;
}

void CommandHistory::clear() {
    undo_.clear();
    redo_.clear();
}

// ---------------------------------------------------------------- Game

Game::Game(std::shared_ptr<const PieceFactory> factory)
    : factory_(std::move(factory)), board_(std::make_unique<Board>(factory_)) {
    newGame();
}

void Game::newGame() {
    board_->setupStandard();
    resetHistory();
}

void Game::loadFen(const std::string& fen) {
    auto board = std::make_unique<Board>(factory_);
    board->loadFen(fen);  // throws before anything changes if the FEN is bad
    board_ = std::move(board);
    resetHistory();
}

void Game::resetHistory() {
    moves_.clear();
    san_.clear();
    captured_[0].clear();
    captured_[1].clear();
    repetitions_.clear();
    repetitions_[board_->positionKey()] = 1;
    history_.clear();
    refresh();
}

bool Game::isOver() const {
    return status_ != GameStatus::Playing && status_ != GameStatus::Check;
}

std::string Game::result() const {
    if (status_ == GameStatus::Checkmate) {
        return board_->sideToMove() == Color::White ? "0-1" : "1-0";
    }
    return isOver() ? "1/2-1/2" : "";
}

bool Game::tryMove(int from, int to, char promotion, std::string& error) {
    if (isOver()) {
        error = "The game is over";
        return false;
    }
    for (const Move& move : legal_) {
        if (move.matches(from, to, promotion)) {
            playMove(move);
            return true;
        }
    }
    error = "Illegal move";
    return false;
}

void Game::playMove(const Move& move) {
    history_.execute(std::make_unique<MoveCommand>(move), *this);
}

void Game::performMove(const Move& move) {
    std::string san = sanWithoutSuffix(move);

    std::unique_ptr<Piece> captured = board_->applyMove(move);
    if (captured) captured_[colorIndex(opposite(captured->color()))].push_back(captured->code());

    moves_.push_back(move);
    ++repetitions_[board_->positionKey()];
    refresh();

    if (status_ == GameStatus::Checkmate) san += '#';
    else if (board_->inCheck(board_->sideToMove())) san += '+';
    san_.push_back(san);
}

void Game::refresh() {
    legal_ = board_->legalMoves();
    const bool check = board_->inCheck(board_->sideToMove());

    if (legal_.empty()) {
        status_ = check ? GameStatus::Checkmate : GameStatus::Stalemate;
    } else if (repetitions_[board_->positionKey()] >= 3) {
        status_ = GameStatus::DrawRepetition;
    } else if (board_->halfmoveClock() >= 100) {
        status_ = GameStatus::DrawFiftyMoves;
    } else {
        status_ = check ? GameStatus::Check : GameStatus::Playing;
    }
    if (isOver()) legal_.clear();
}

std::string Game::sanWithoutSuffix(const Move& move) const {
    if (move.isCastling()) return Square::col(move.rookFrom) == 7 ? "O-O" : "O-O-O";

    const Piece* piece = board_->pieceAt(move.from);
    std::string san = piece->sanLetter();

    if (san.empty()) {
        // Pawn: captures are written with the file it came from ("exd5").
        if (move.isCapture()) san += Square::name(move.from)[0];
    } else {
        // Disambiguation when two identical pieces can reach the same square ("Nbd7", "R1e2").
        bool clash = false, sameFile = false, sameRank = false;
        for (const Move& other : legal_) {
            if (other.to != move.to || other.from == move.from) continue;
            if (board_->pieceAt(other.from)->code() != piece->code()) continue;
            clash = true;
            sameFile = sameFile || Square::col(other.from) == Square::col(move.from);
            sameRank = sameRank || Square::row(other.from) == Square::row(move.from);
        }
        if (clash) {
            const std::string from = Square::name(move.from);
            if (!sameFile) san += from[0];
            else if (!sameRank) san += from[1];
            else san += from;
        }
    }

    if (move.isCapture()) san += 'x';
    san += Square::name(move.to);
    if (move.promotion != 0) {
        san += '=';
        san += move.promotion;
    }
    return san;
}

std::unique_ptr<GameMemento> Game::saveState() const {
    std::unique_ptr<GameMemento> memento(new GameMemento());
    memento->board_ = board_->clone();
    memento->moves_ = moves_;
    memento->san_ = san_;
    memento->captured_[0] = captured_[0];
    memento->captured_[1] = captured_[1];
    memento->repetitions_ = repetitions_;
    return memento;
}

void Game::restoreState(const GameMemento& memento) {
    board_ = memento.board_->clone();  // the memento stays intact for the next undo/redo
    moves_ = memento.moves_;
    san_ = memento.san_;
    captured_[0] = memento.captured_[0];
    captured_[1] = memento.captured_[1];
    repetitions_ = memento.repetitions_;
    refresh();
}
