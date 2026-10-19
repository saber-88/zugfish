#pragma once

#include "game.hpp"
#include "uci_engine.hpp"
#include <functional>
#include <iostream>
#include <string>
#include <vector>

#include "opening_book.hpp"

namespace zugfish {

enum class MoveClassification {
    Brilliant,
    Great,
    Best,
    Excellent,
    Good,
    Book,
    Inaccuracy,
    Mistake,
    Blunder,
    Miss,
    Forced
};

std::string to_string(MoveClassification c);
std::string to_symbol(MoveClassification c);

// One move in a learning continuation, including the resulting position evaluation.
struct LearningPly {
    Ply ply;
    std::string fen_after;
    std::string best_uci; // best reply from the resulting position
    Score score_white_perspective;
    double win_percent_white = 50.0;
};

struct AnalyzedPly {
    Ply ply;
    EngineEvaluation before_eval;
    EngineEvaluation after_eval;
    std::string best_was_san;      // Best alternative move in fen_before formatted as SAN (e.g. "Na6")
    std::string next_best_san;     // Engine recommendation in fen_after formatted as SAN (e.g. "Nxb5")
    Score score_white_perspective; // Normalized score after this move from White's perspective
    double win_percent_white = 50.0; // White winning chance after this move [0.0, 100.0]
    double win_percent_loss = 0.0; // [0.0, 1.0] lost by side to move
    int centipawn_loss = 0;
    MoveClassification classification = MoveClassification::Good;
    // Populated only when the mover's win probability drops by more than 10 points.
    // Each line starts from ply.fen_before and contains up to 20 plies (10 moves per side).
    std::vector<LearningPly> best_line;
    std::vector<LearningPly> played_line;
};

struct PlayerStats {
    double accuracy = 0.0; // 0.0% to 100.0%
    double opening_accuracy = 0.0;
    double middlegame_accuracy = 0.0;
    double endgame_accuracy = 0.0;
    double acpl = 0.0;     // Average Centipawn Loss
    int estimated_rating = 0; // Performance / Game rating (Chess.com style)
    int brilliant_count = 0;
    int great_count = 0;
    int best_count = 0;
    int excellent_count = 0;
    int good_count = 0;
    int book_count = 0;
    int inaccuracy_count = 0;
    int mistake_count = 0;
    int blunder_count = 0;
    int miss_count = 0;
    int forced_count = 0;
};

struct GameReview {
    std::map<std::string, std::string> headers;
    OpeningInfo opening;
    EngineEvaluation initial_eval; // Evaluation before move 0
    std::string initial_best_san;  // Suggested opening move in SAN (e.g. "e4")
    std::vector<AnalyzedPly> plies;
    PlayerStats white_stats;
    PlayerStats black_stats;
};

struct ReviewProgress {
    size_t current_ply = 0;
    size_t total_plies = 0;
    std::string move_san;
    MoveClassification last_classification = MoveClassification::Best;
    double current_accuracy_white = 0.0;
    double current_accuracy_black = 0.0;
};

using ProgressCallback = std::function<void(const ReviewProgress&)>;
using CancelCallback = std::function<bool()>;

class GameReviewer {
public:
#ifdef _WIN32
    explicit GameReviewer(const std::string& engine_path = "stockfish.exe");
#else
    explicit GameReviewer(const std::string& engine_path = "/usr/sbin/stockfish");
#endif
    explicit GameReviewer(UciEngine engine);

    GameReview review(const Game& game, int depth = 12, ProgressCallback progress_cb = nullptr, CancelCallback cancel_cb = nullptr);

    // Formatted printer for game reviews
    static void print_review(const GameReview& review, std::ostream& os = std::cout);

private:
    UciEngine engine_;
};

} // namespace zugfish
