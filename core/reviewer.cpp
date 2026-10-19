#include "reviewer.hpp"
#include <chess.hpp>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <sstream>

namespace zugfish {

std::string to_string(MoveClassification c) {
    switch (c) {
        case MoveClassification::Brilliant: return "Brilliant";
        case MoveClassification::Great:     return "Great";
        case MoveClassification::Best:      return "Best";
        case MoveClassification::Excellent: return "Excellent";
        case MoveClassification::Good:      return "Good";
        case MoveClassification::Book:      return "Book";
        case MoveClassification::Inaccuracy:return "Inaccuracy";
        case MoveClassification::Mistake:   return "Mistake";
        case MoveClassification::Blunder:   return "Blunder";
        case MoveClassification::Miss:      return "Miss";
        case MoveClassification::Forced:    return "Forced";
    }
    return "Unknown";
}

std::string to_symbol(MoveClassification c) {
    switch (c) {
        case MoveClassification::Brilliant: return "!!";
        case MoveClassification::Great:     return "!";
        case MoveClassification::Best:      return "[*]";
        case MoveClassification::Excellent: return "[!]";
        case MoveClassification::Good:      return "[+]";
        case MoveClassification::Book:      return "[B]";
        case MoveClassification::Inaccuracy:return "[?!]";
        case MoveClassification::Mistake:   return "[?]";
        case MoveClassification::Blunder:   return "[??]";
        case MoveClassification::Miss:      return "[X]";
        case MoveClassification::Forced:    return "[]";
    }
    return "";
}

GameReviewer::GameReviewer(const std::string& engine_path)
    : engine_(engine_path) {}

GameReviewer::GameReviewer(UciEngine engine)
    : engine_(std::move(engine)) {}

static void update_stats(PlayerStats& stats, MoveClassification c) {
    switch (c) {
        case MoveClassification::Brilliant: stats.brilliant_count++; break;
        case MoveClassification::Great:     stats.great_count++; break;
        case MoveClassification::Best:      stats.best_count++; break;
        case MoveClassification::Excellent: stats.excellent_count++; break;
        case MoveClassification::Good:      stats.good_count++; break;
        case MoveClassification::Book:      stats.book_count++; break;
        case MoveClassification::Inaccuracy: stats.inaccuracy_count++; break;
        case MoveClassification::Mistake:   stats.mistake_count++; break;
        case MoveClassification::Blunder:   stats.blunder_count++; break;
        case MoveClassification::Miss:      stats.miss_count++; break;
        case MoveClassification::Forced:    stats.forced_count++; break;
    }
}

static inline int piece_val(chess::PieceType pt) {
    switch (pt.internal()) {
        case chess::PieceType::underlying::PAWN:   return 1;
        case chess::PieceType::underlying::KNIGHT: return 3;
        case chess::PieceType::underlying::BISHOP: return 3;
        case chess::PieceType::underlying::ROOK:   return 5;
        case chess::PieceType::underlying::QUEEN:  return 9;
        default: return 0;
    }
}

static int total_material(const chess::Board& board, chess::Color color) {
    int sum = 0;
    for (int sq = 0; sq < 64; ++sq) {
        chess::Piece p = board.at(chess::Square(sq));
        if (p != chess::Piece::NONE && p.color() == color) {
            sum += piece_val(p.type());
        }
    }
    return sum;
}

static int non_pawn_material(const chess::Board& board) {
    int sum = 0;
    for (int sq = 0; sq < 64; ++sq) {
        chess::Piece p = board.at(chess::Square(sq));
        if (p != chess::Piece::NONE && p.type() != chess::PieceType::PAWN && p.type() != chess::PieceType::KING) {
            sum += piece_val(p.type());
        }
    }
    return sum;
}

static int count_queens(const chess::Board& board) {
    int count = 0;
    for (int sq = 0; sq < 64; ++sq) {
        chess::Piece p = board.at(chess::Square(sq));
        if (p != chess::Piece::NONE && p.type() == chess::PieceType::QUEEN) {
            count++;
        }
    }
    return count;
}

static inline int to_cp(const Score& s) {
    if (s.is_mate) {
        return s.value > 0 ? 1000 : -1000;
    }
    return std::clamp(s.value, -1000, 1000);
}

static std::string uci_to_san(const std::string& fen, const std::string& uci) {
    if (uci.empty() || uci == "(none)") return "";
    try {
        chess::Board board(fen);
        chess::Move m = chess::uci::uciToMove(board, uci);
        return chess::uci::moveToSan(board, m);
    } catch (...) {
        return uci;
    }
}

static std::vector<LearningPly> build_learning_line(UciEngine& engine,
                                                        const std::string& start_fen,
                                                        const std::string& first_uci,
                                                        int depth,
                                                        const CancelCallback& cancel_cb) {
    std::vector<LearningPly> line;
    if (first_uci.empty() || first_uci == "(none)")
        return line;
struct MoveEvalRecord {
    double accuracy = 100.0;
    double weight = 1.0;
    int phase = 0; // 0 = Opening, 1 = Middlegame, 2 = Endgame
};

static double compute_phase_accuracy(const std::vector<MoveEvalRecord>& records, int target_phase = -1) {
    double sum_w = 0.0;
    double sum_weighted_acc = 0.0;
    double sum_harmonic = 0.0;

    for (const auto& r : records) {
        if (target_phase >= 0 && r.phase != target_phase) {
            continue;
        }
        sum_w += r.weight;
        sum_weighted_acc += r.weight * r.accuracy;
        sum_harmonic += r.weight / std::max(r.accuracy, 12.0);
    }

    if (sum_w <= 0.0) return (target_phase >= 0) ? 0.0 : 100.0;

    double arith = sum_weighted_acc / sum_w;
    double harmonic = sum_w / sum_harmonic;
    double combined = 0.80 * arith + 0.20 * harmonic;
    return std::clamp(combined, 0.0, 100.0);
}

    try {
        chess::Board board(start_fen);
        EngineEvaluation position_eval;
        std::string chosen_uci = first_uci;

        // Twenty plies means ten moves for each side (White and Black), including the candidate move.
        for (int step = 0; step < 20; ++step) {
            if (cancel_cb && cancel_cb())
                break;
            if (chosen_uci.empty() || chosen_uci == "(none)")
                break;

            LearningPly item;
            item.ply.fen_before = board.getFen();
            item.ply.uci = chosen_uci;
            const chess::Move move = chess::uci::uciToMove(board, chosen_uci);
            item.ply.san = chess::uci::moveToSan(board, move);
            board.makeMove(move);
            item.fen_after = board.getFen();

            engine.set_position_fen(item.fen_after);
            position_eval = engine.analyze_depth(depth);
            const bool white_to_move_after = item.fen_after.find(" w ") != std::string::npos;
            item.best_uci = position_eval.best_move;
            item.score_white_perspective = position_eval.score.to_white_perspective(white_to_move_after);
            item.win_percent_white = position_eval.score.win_probability(white_to_move_after) * 100.0;
            line.push_back(std::move(item));

            // The best move in the evaluated resulting position becomes the next ply.
            chosen_uci = position_eval.best_move;
        }
    } catch (...) {
        // Keep any valid prefix; a malformed/terminal position must not fail the full review.
    }
    return line;
}

static double compute_game_accuracy(const std::vector<double>& decision_accs,
                                    const std::vector<double>& all_accs) {
    const auto& accs = decision_accs.empty() ? all_accs : decision_accs;
    if (accs.empty()) return 100.0;
static int calculate_estimated_rating(double accuracy, double acpl,
                                      const PlayerStats& stats,
                                      int actual_elo) {
    if (actual_elo >= 100 && actual_elo <= 3800) {
        // Model expected accuracy at player's rating level
        double expected_acc = 50.0 + 38.0 / (1.0 + std::exp(-(actual_elo - 1100.0) / 450.0));
        double delta_acc = accuracy - expected_acc;

        // Base rating offset: ~22 rating points per 1% accuracy difference
        double perf = actual_elo + delta_acc * 22.0;

        // Tactical and error adjustments
        perf += stats.brilliant_count * 50.0;
        perf += stats.great_count * 30.0;
        perf -= stats.blunder_count * 40.0;
        perf -= stats.miss_count * 20.0;
        perf -= stats.mistake_count * 10.0;

        // Clean play bonus: zero blunders in a substantial game
        if (stats.blunder_count == 0 && (stats.best_count + stats.excellent_count >= 10)) {
            perf += 60.0;
        }

        // Single-game variance clamped realistically
        double min_bound = std::max(100.0, actual_elo - 450.0);
        double max_bound = std::min(3200.0, actual_elo + 550.0);
        perf = std::clamp(perf, min_bound, max_bound);

    double sum = std::accumulate(accs.begin(), accs.end(), 0.0);
    double arith_mean = sum / accs.size();

    double harmonic_sum = 0.0;
    for (double a : accs) {
        harmonic_sum += 1.0 / std::max(a, 1.0);
    // Historical or unrated games without Elo header
    double base = 0.0;
    if (accuracy >= 95.0) {
        base = 2400.0 + (accuracy - 95.0) * 100.0;
    } else if (accuracy >= 88.0) {
        base = 1900.0 + (accuracy - 88.0) * 70.0;
    } else if (accuracy >= 78.0) {
        base = 1400.0 + (accuracy - 78.0) * 50.0;
    } else if (accuracy >= 65.0) {
        base = 1000.0 + (accuracy - 65.0) * 30.0;
    } else if (accuracy >= 50.0) {
        base = 650.0 + (accuracy - 50.0) * 23.0;
    } else {
        base = std::max(100.0, 200.0 + accuracy * 9.0);
    }
    double harmonic_mean = static_cast<double>(accs.size()) / harmonic_sum;

    // Chess.com CAPS2 weighting: 25% arithmetic + 75% harmonic mean.
    // Gives realistic 40-65% scores for games with multiple blunders/mistakes,
    // while preserving 92-99% on masterly played games.
    double blended = (0.25 * arith_mean) + (0.75 * harmonic_mean);
    return std::clamp(blended, 0.0, 100.0);
}

// Check if a move is a genuine piece sacrifice (Brilliant !!)
static bool check_brilliant_sacrifice(
    size_t ply_idx,
    const std::string& uci,
    const EngineEvaluation& prev_eval,
    const EngineEvaluation& next_eval,
    const chess::Board& board_before,
    const std::string& next_fen,
    bool white_to_move,
    [[maybe_unused]] double player_win_before,
    double player_win_after,
    double win_loss,
    int material_lead,
    const Game& game)
{
    // Must be top engine move
    if (uci != prev_eval.best_move) return false;
    // Beyond opening development (move 7+)
    if (ply_idx < 12) return false;
    // Cannot already hold overwhelming advantage in material
    if (material_lead > 2) return false;
    // Sound position: must be at least equal or better
    if (player_win_after < 0.45 || win_loss > 0.012) return false;

    chess::Color player_c = white_to_move ? chess::Color::WHITE : chess::Color::BLACK;
    chess::Color opp_c    = white_to_move ? chess::Color::BLACK : chess::Color::WHITE;

    try {
        chess::Move move_played = chess::uci::uciToMove(board_before, uci);
        chess::Piece moved_piece = board_before.at(move_played.from());
        chess::Square to_sq = move_played.to();
        int moved_val = piece_val(moved_piece.type());
        chess::Piece cap_piece = board_before.at(to_sq);
        int cap_val = (cap_piece != chess::Piece::NONE) ? piece_val(cap_piece.type()) : 0;
        int net_sac = moved_val - cap_val;

        chess::Board b_after(next_fen);
        // Exclude immediate checkmate delivery
        if (b_after.isGameOver().first != chess::GameResultReason::NONE) {
            return false;
        }

        bool opp_recaptures = false;
        int opp_cap_val = 0;
        if (!next_eval.best_move.empty() && next_eval.best_move != "(none)") {
            try {
                chess::Move opp_best = chess::uci::uciToMove(b_after, next_eval.best_move);
                if (opp_best.to() == to_sq) {
                    opp_recaptures = true;
                    chess::Piece opp_p = b_after.at(opp_best.from());
                    if (opp_p != chess::Piece::NONE) {
                        opp_cap_val = piece_val(opp_p.type());
                    }
                }
            } catch (...) {}
        }
        if (!opp_recaptures && ply_idx + 1 < game.plies.size()) {
            try {
                chess::Move opp_actual = chess::uci::uciToMove(b_after, game.plies[ply_idx + 1].uci);
                if (opp_actual.to() == to_sq) {
                    opp_recaptures = true;
                    chess::Piece opp_p = b_after.at(opp_actual.from());
                    if (opp_p != chess::Piece::NONE) {
                        opp_cap_val = piece_val(opp_p.type());
                    }
                }
            } catch (...) {}
        }

        // Direct sacrifice:
        // Moved piece value >= 3 (minor, rook, queen)
        if (moved_val >= 3 && b_after.isAttacked(to_sq, opp_c)) {
            // Case 1: Capture with net material sacrifice (e.g. R(5) for N(3) net_sac=2, or N(3) for P(1) net_sac=2)
            if (cap_piece != chess::Piece::NONE) {
                if (net_sac >= 2 && opp_recaptures) {
                    return true;
                }
            } else {
                // Case 2: Move to an empty square en prise
                bool square_defended = b_after.isAttacked(to_sq, player_c);
                if (opp_recaptures) {
                    // It is a sacrifice if captured by a strictly lesser piece (e.g. Q for R/N/B/P, or R/N/B for P),
                    // OR if the piece was left completely undefended on the square
                    if (opp_cap_val < moved_val || !square_defended) {
                        return true;
                    }
                } else {
                    // Left completely hanging and undefended
                    if (!square_defended) {
                        return true;
                    }
                }
            }
        }

        // Deflection / Quiet sacrifice:
        // A different major/minor piece of the player (>= 3) is left hanging and undefended,
        // and opponent's top move captures that hanging piece
        if (!opp_recaptures && !next_eval.best_move.empty() && next_eval.best_move != "(none)") {
            chess::Move opp_best = chess::uci::uciToMove(b_after, next_eval.best_move);
            chess::Piece target = b_after.at(opp_best.to());
            if (target != chess::Piece::NONE && target.color() == player_c && piece_val(target.type()) >= 3) {
                if (!b_after.isAttacked(opp_best.to(), player_c)) {
                    bool was_hanging_before = !board_before.isAttacked(opp_best.to(), player_c) &&
                                              board_before.isAttacked(opp_best.to(), opp_c);
                    if (!was_hanging_before) {
                        return true;
                    }
                }
            }
        }
    } catch (...) {}

    return false;
}

// Check if a move is the only good move in the position (Great !)
static bool check_great_move(
    size_t ply_idx,
    const std::string& uci,
    const EngineEvaluation& prev_eval,
    const chess::Board& board_before,
    const std::string& next_fen,
    bool white_to_move,
    double player_win_before,
    double player_win_after,
    int material_lead,
    const Game& game)
{
    if (uci != prev_eval.best_move) return false;
    if (ply_idx < 8) return false;
    if (material_lead > 6) return false;
    if (!prev_eval.has_second_best) return false;

    try {
        chess::Move move_played = chess::uci::uciToMove(board_before, uci);
        chess::Square to_sq = move_played.to();
        chess::Piece cap_piece = board_before.at(to_sq);
        chess::Board b_after(next_fen);
        chess::Color opp_c = white_to_move ? chess::Color::BLACK : chess::Color::WHITE;

        // Exclude immediate checkmate
        if (b_after.isGameOver().first != chess::GameResultReason::NONE) return false;

        // Exclude recaptures
        if (ply_idx > 0) {
            chess::Board prev_board(game.plies[ply_idx - 1].fen_before);
            chess::Move prev_opp_move = chess::uci::uciToMove(prev_board, game.plies[ply_idx - 1].uci);
            if (to_sq == prev_opp_move.to()) return false;
        }

        // Exclude simple check evasion
        if (board_before.inCheck()) return false;

        // Exclude capturing an undefended hanging piece for free
        if (cap_piece != chess::Piece::NONE && !b_after.isAttacked(to_sq, opp_c)) return false;

        double score2_p = prev_eval.second_score.win_probability(white_to_move);
        double player_win_2 = white_to_move ? score2_p : (1.0 - score2_p);
        double win_gap = player_win_before - player_win_2;

        int cp_before = white_to_move ? to_cp(prev_eval.score.to_white_perspective(true))
                                      : -to_cp(prev_eval.score.to_white_perspective(false));
        int cp2_val   = white_to_move ? to_cp(prev_eval.second_score.to_white_perspective(true))
                                      : -to_cp(prev_eval.second_score.to_white_perspective(false));
        int cp_gap = cp_before - cp2_val;

        if (win_gap >= 0.12 || cp_gap >= 120) {
            // Scenario 1: Sole defensive resource holding a tough position
            if (player_win_before <= 0.55 && player_win_after >= 0.40 && player_win_2 <= 0.25) {
                return true;
            }
            // Scenario 2: Only winning breakthrough move in a sharp or contested position
            if (player_win_before >= 0.45 && player_win_after >= 0.58 &&
                (win_gap >= 0.16 || cp_gap >= 160 || player_win_2 <= 0.48)) {
                return true;
            }
        }
    } catch (...) {}

    return false;
}

GameReview GameReviewer::review(const Game& game, int depth, ProgressCallback progress_cb, CancelCallback cancel_cb) {
    GameReview review;
    review.headers = game.headers;

    if (game.plies.empty()) {
        return review;
    }

    engine_.new_game();

    // Determine the position after the final move
    std::string after_last_fen;
    try {
        chess::Board last_board(game.plies.back().fen_before);
        chess::Move m = chess::uci::uciToMove(last_board, game.plies.back().uci);
        last_board.makeMove(m);
        after_last_fen = last_board.getFen();
    } catch (...) {
        after_last_fen = game.plies.back().fen_before;
    }

    // Evaluate initial position before move 0
    engine_.set_position_fen(game.plies[0].fen_before);
    auto prev_eval = engine_.analyze_depth(depth);
    review.initial_eval = prev_eval;
    review.initial_best_san = uci_to_san(game.plies[0].fen_before, prev_eval.best_move);

    std::vector<MoveEvalRecord> white_records;
    std::vector<MoveEvalRecord> black_records;
    std::vector<int> white_cpls;
    std::vector<int> black_cpls;

    std::vector<std::string> uci_history;
    bool still_in_book = true;

    for (size_t i = 0; i < game.plies.size(); ++i) {
        if (cancel_cb && cancel_cb()) {
            break;
        }

        const auto& ply = game.plies[i];
        bool white_to_move = (ply.fen_before.find(" w ") != std::string::npos);

        std::string next_fen = (i + 1 < game.plies.size()) 
                               ? game.plies[i + 1].fen_before 
                               : after_last_fen;

        engine_.set_position_fen(next_fen);
        auto next_eval = engine_.analyze_depth(depth);

        // Win probabilities from White's perspective
        double win_before_white = prev_eval.score.win_probability(white_to_move);
        double win_after_white  = next_eval.score.win_probability(!white_to_move);

        // Win probabilities from moving player's perspective
        double player_win_before = white_to_move ? win_before_white : (1.0 - win_before_white);
        double player_win_after  = white_to_move ? win_after_white  : (1.0 - win_after_white);

        double win_loss = (ply.uci == prev_eval.best_move)
                          ? 0.0
                          : std::max(0.0, player_win_before - player_win_after);

        // Centipawn loss
        Score score_before_white = prev_eval.score.to_white_perspective(white_to_move);
        Score score_after_white  = next_eval.score.to_white_perspective(!white_to_move);

        int cp_before = white_to_move ? to_cp(score_before_white) : -to_cp(score_before_white);
        int cp_after  = white_to_move ? to_cp(score_after_white)  : -to_cp(score_after_white);
        int cpl = (ply.uci == prev_eval.best_move) ? 0 : std::max(0, cp_before - cp_after);

        chess::Board board_before(ply.fen_before);
        chess::Color player_c = white_to_move ? chess::Color::WHITE : chess::Color::BLACK;
        chess::Color opp_c    = white_to_move ? chess::Color::BLACK : chess::Color::WHITE;
        int mat_player = total_material(board_before, player_c);
        int mat_opp    = total_material(board_before, opp_c);
        int material_lead = mat_player - mat_opp;

        // 1. Check Forced Move (only 1 legal move)
        chess::Movelist legals;
        chess::movegen::legalmoves(legals, board_before);
        bool is_forced = (legals.size() == 1);

        // 2. Check Book Move (Exhaustive Polyglot + ECO Database)
        bool is_book = false;
        if (still_in_book) {
            if (OpeningBook::instance().is_book_move(board_before, uci_history, ply.uci)) {
                is_book = true;
            } else {
                still_in_book = false;
            }
        }
        uci_history.push_back(ply.uci);

        // 3. Check Brilliant Sacrifice (!!)
        bool is_best_move = (ply.uci == prev_eval.best_move);
        bool is_brilliant = false;
        
        chess::Color player_c = white_to_move ? chess::Color::WHITE : chess::Color::BLACK;
        chess::Color opp_c    = white_to_move ? chess::Color::BLACK : chess::Color::WHITE;
        int mat_player = total_material(board_before, player_c);
        int mat_opp    = total_material(board_before, opp_c);
        int material_lead = mat_player - mat_opp;

        // Strict Criteria for Brilliant (!!):
        // a) Must be the top engine move
        // b) Not a book move, not forced
        // c) Must not already have overwhelming material dominance (material_lead <= 3 pawns)
        // d) Position must be completely sound after (player_win_after >= 0.50 and win_loss <= 0.01)
        // e) Must involve an intentional piece sacrifice directly initiated by this move
        if (is_best_move && !is_book && !is_forced && 
            material_lead <= 3 && player_win_after >= 0.50 && win_loss <= 0.01) {

            try {
                chess::Move move_played = chess::uci::uciToMove(board_before, ply.uci);
                chess::Piece moved_piece = board_before.at(move_played.from());
                chess::Square to_sq = move_played.to();
                int moved_val = piece_val(moved_piece.type());

                // Pawn pushes are not piece sacrifices; only minor pieces, rooks, queens
                if (moved_val >= 3) {
                    chess::Board b_after(next_fen);
                    // Check if opponent's best response captures the sacrificed piece
                    if (!next_eval.best_move.empty() && next_eval.best_move != "(none)") {
                        chess::Move opp_move = chess::uci::uciToMove(b_after, next_eval.best_move);

                        // Opponent captures the moved piece on its destination square
                        if (opp_move.to() == to_sq && b_after.isCapture(opp_move)) {
                            // Net material sacrificed in the exchange
                            // e.g. Morphy 13. Rxd7 (Rook for Knight, 5 - 3 = 2)
                            // e.g. Morphy 16. Qb8+ (Queen for 0, 9 - 0 = 9)
                            chess::Piece cap_piece = board_before.at(to_sq);
                            int cap_val = (cap_piece != chess::Piece::NONE) ? piece_val(cap_piece.type()) : 0;
                            int net_sac = moved_val - cap_val;

                            if (net_sac >= 2) {
                                is_brilliant = true;
                            }
                        }
                    }
                }
            } catch (...) {}
        if (!is_book && !is_forced) {
            is_brilliant = check_brilliant_sacrifice(
                i, ply.uci, prev_eval, next_eval, board_before, next_fen,
                white_to_move, player_win_before, player_win_after, win_loss, material_lead, game);
        }

        // 4. Check Great Move (!)
        bool is_great = false;
        if (is_best_move && !is_book && !is_forced && !is_brilliant) {
            // A Great move is a critical move that creates a decisive winning breakthrough
            // (>= 12% win chance jump to >= 70%) or finds the only saving defense from a losing position
            if ((player_win_before <= 0.72 && player_win_after >= 0.72 && (player_win_after - player_win_before) >= 0.12) ||
                (player_win_before <= 0.30 && player_win_after >= 0.45 && (player_win_after - player_win_before) >= 0.15)) {
                is_great = true;
            }
        if (!is_book && !is_forced && !is_brilliant) {
            is_great = check_great_move(
                i, ply.uci, prev_eval, board_before, next_fen,
                white_to_move, player_win_before, player_win_after, material_lead, game);
        }

        // 5. Standard Classification Pipeline
        MoveClassification classification;
        if (is_book) {
            classification = MoveClassification::Book;
        } else if (is_forced) {
            classification = MoveClassification::Forced;
        } else if (is_brilliant) {
            classification = MoveClassification::Brilliant;
        } else if (is_great) {
            classification = MoveClassification::Great;
        } else if (is_best_move || win_loss <= 0.005) {
            classification = MoveClassification::Best;
        } else if (win_loss <= 0.025) {
            classification = MoveClassification::Excellent;
        } else if (win_loss <= 0.060) {
            classification = MoveClassification::Good;
        } else if (win_loss <= 0.130) {
            classification = MoveClassification::Inaccuracy;
        } else if (win_loss <= 0.250) {
            classification = MoveClassification::Mistake;
        } else {
            if (player_win_before >= 0.75) {
        } else if (win_loss <= 0.220) {
            classification = MoveClassification::Mistake;
        } else {
            // Missed Win vs Blunder:
            // If player held a decisive winning advantage (>= 0.75 win prob) and lost it (<= 0.60)
            if (player_win_before >= 0.75 && player_win_after <= 0.60) {
                classification = MoveClassification::Miss;
            } else {
                classification = MoveClassification::Blunder;
            }
        }

        // Accuracy score for this single move [0.0, 100.0] via CAPS2 exponential decay formula:
        // Best moves score 100%, minor inaccuracies score 70-80%, mistakes score 20-35%,
        // and blunders/misses drop to 0-10%.
        double delta_win_pct = std::max(0.0, win_loss * 100.0);
        double move_accuracy = 100.0;
        if (delta_win_pct > 0.0) {
            double raw = 103.0 * std::exp(-0.065 * delta_win_pct) - 3.0;
            move_accuracy = std::clamp(raw, 0.0, 100.0);
        // Decisive Advantage Rule:
        // When strongly winning both before (>= 0.80) and after (>= 0.65),
        // picking a slower path to victory is never marked as Mistake, Blunder, or Miss
        if ((classification == MoveClassification::Mistake || 
             classification == MoveClassification::Blunder || 
             classification == MoveClassification::Miss) &&
            player_win_before >= 0.80 && player_win_after >= 0.65) {
            classification = (win_loss <= 0.12) ? MoveClassification::Good : MoveClassification::Inaccuracy;
        }

        // Forced mate protection:
        // If the resulting position forces checkmate against opponent, never classify as error
        bool mates_opp = (white_to_move && score_after_white.is_mate && score_after_white.value > 0) ||
                         (!white_to_move && score_after_white.is_mate && score_after_white.value < 0);
        if (mates_opp && (classification == MoveClassification::Mistake || 
                          classification == MoveClassification::Blunder || 
                          classification == MoveClassification::Miss ||
                          classification == MoveClassification::Inaccuracy)) {
            classification = MoveClassification::Best;
        }

        // CAPS2 Move Accuracy Curve
        double delta_win_pct = std::max(0.0, win_loss * 100.0);
        double move_accuracy = 100.0;
        if (classification == MoveClassification::Book || 
            classification == MoveClassification::Forced ||
            classification == MoveClassification::Brilliant || 
            classification == MoveClassification::Great || 
            classification == MoveClassification::Best) {
            move_accuracy = 100.0;
        } else if (classification == MoveClassification::Excellent) {
            double raw = 100.0 - (delta_win_pct * 2.0);
            move_accuracy = std::clamp(raw, 90.0, 98.0);
        } else if (classification == MoveClassification::Good) {
            double raw = 88.0 - ((delta_win_pct - 2.5) * 2.2);
            move_accuracy = std::clamp(raw, 72.0, 88.0);
        } else if (classification == MoveClassification::Inaccuracy) {
            double raw = 68.0 - ((delta_win_pct - 6.5) * 1.6);
            move_accuracy = std::clamp(raw, 45.0, 68.0);
        } else if (classification == MoveClassification::Mistake) {
            double raw = 40.0 - ((delta_win_pct - 13.0) * 1.2);
            move_accuracy = std::clamp(raw, 18.0, 40.0);
        } else if (classification == MoveClassification::Miss) {
            double raw = 18.0 - ((delta_win_pct - 22.0) * 0.5);
            move_accuracy = std::clamp(raw, 5.0, 18.0);
        } else { // Blunder
            double raw = 10.0 * std::exp(-0.06 * (delta_win_pct - 22.0));
            move_accuracy = std::clamp(raw, 0.0, 10.0);
        }

        // Determine Game Phase
        int move_number = static_cast<int>(i / 2) + 1;
        int non_pawn_mat = non_pawn_material(board_before);
        int queens = count_queens(board_before);

        int current_phase = 1; // Middlegame
        if (move_number <= 10 && still_in_book) {
            current_phase = 0; // Opening
        } else if (move_number <= 12 && non_pawn_mat >= 38) {
            current_phase = 0; // Opening
        } else if (non_pawn_mat <= 20 || (queens == 0 && non_pawn_mat <= 26)) {
            current_phase = 2; // Endgame
        } else {
            current_phase = 1; // Middlegame
        }

        // Move Weighting
        double move_weight = 1.0;
        if (is_book || is_forced) {
            move_weight = 0.15;
        } else {
            bool is_decided = (player_win_before >= 0.94 && material_lead >= 6) ||
                              (player_win_before <= 0.06 && material_lead <= -6);
            if (is_decided && classification != MoveClassification::Blunder && 
                classification != MoveClassification::Miss) {
                move_weight = 0.25;
            } else if (classification == MoveClassification::Blunder) {
                move_weight = 1.35;
            } else if (classification == MoveClassification::Miss) {
                move_weight = 1.25;
            } else if (classification == MoveClassification::Mistake) {
                move_weight = 1.15;
            } else {
                move_weight = 1.00;
            }
        }

        MoveEvalRecord record{move_accuracy, move_weight, current_phase};
        if (white_to_move) {
            white_records.push_back(record);
            white_cpls.push_back(cpl);
            update_stats(review.white_stats, classification);
        } else {
            black_records.push_back(record);
            black_cpls.push_back(cpl);
            update_stats(review.black_stats, classification);
        }

        AnalyzedPly analyzed;
        analyzed.ply = ply;
        analyzed.before_eval = prev_eval;
        analyzed.after_eval = next_eval;
        analyzed.best_was_san = uci_to_san(ply.fen_before, prev_eval.best_move);
        analyzed.next_best_san = uci_to_san(next_fen, next_eval.best_move);
        analyzed.score_white_perspective = score_after_white;
        analyzed.win_percent_white = win_after_white * 100.0;
        analyzed.win_percent_loss = win_loss;
        analyzed.centipawn_loss = cpl;
        analyzed.classification = classification;

        // Add a pair of engine-generated teaching lines only for a >10 percentage-point
        // drop in the moving player's win probability. Both lines start before the move.
        if (win_loss > 0.10 && !prev_eval.best_move.empty() && prev_eval.best_move != ply.uci) {
            analyzed.best_line = build_learning_line(engine_, ply.fen_before, prev_eval.best_move, depth, cancel_cb);
            analyzed.played_line = build_learning_line(engine_, ply.fen_before, ply.uci, depth, cancel_cb);
        }

        review.plies.push_back(analyzed);

        if (progress_cb) {
            ReviewProgress prog;
            prog.current_ply = i + 1;
            prog.total_plies = game.plies.size();
            prog.move_san = ply.san;
            prog.last_classification = classification;
            prog.current_accuracy_white = compute_phase_accuracy(white_records, -1);
            prog.current_accuracy_black = compute_phase_accuracy(black_records, -1);

            progress_cb(prog);
        }

        prev_eval = std::move(next_eval);
    }

    auto avg_int = [](const std::vector<int>& v) {
        if (v.empty()) return 0.0;
        return static_cast<double>(std::accumulate(v.begin(), v.end(), 0)) / v.size();
    };

    review.white_stats.accuracy = compute_phase_accuracy(white_records, -1);
    review.white_stats.opening_accuracy = compute_phase_accuracy(white_records, 0);
    review.white_stats.middlegame_accuracy = compute_phase_accuracy(white_records, 1);
    review.white_stats.endgame_accuracy = compute_phase_accuracy(white_records, 2);
    review.white_stats.acpl = avg_int(white_cpls);

    review.black_stats.accuracy = compute_phase_accuracy(black_records, -1);
    review.black_stats.opening_accuracy = compute_phase_accuracy(black_records, 0);
    review.black_stats.middlegame_accuracy = compute_phase_accuracy(black_records, 1);
    review.black_stats.endgame_accuracy = compute_phase_accuracy(black_records, 2);
    review.black_stats.acpl = avg_int(black_cpls);

    review.opening = OpeningBook::instance().identify_opening(uci_history);

    return review;
}

void GameReviewer::print_review(const GameReview& review, std::ostream& os) {
    auto get_header = [&](const std::string& key, const std::string& def = "?") {
        auto it = review.headers.find(key);
        return it != review.headers.end() ? it->second : def;
    };

    os << "\n================================================================================\n";
    os << "                           ZUGFISH GAME REVIEW\n";
    os << "================================================================================\n";
    os << " White: " << std::left << std::setw(32) << get_header("White")
       << " Black: " << get_header("Black") << "\n";
    if (!review.opening.name.empty()) {
        os << " Opening:  " << review.opening.eco << " - " << review.opening.name << "\n";
    }
    os << " Accuracy: " << std::fixed << std::setprecision(1) << review.white_stats.accuracy << "%"
       << std::setw(26) << ""
       << " Accuracy: " << review.black_stats.accuracy << "%\n";
    os << " ACPL: " << std::setprecision(1) << review.white_stats.acpl
       << std::setw(30) << ""
       << " ACPL: " << review.black_stats.acpl << "\n";
    os << "--------------------------------------------------------------------------------\n";
    os << " Phase Accuracy                            White              Black\n";
    os << "--------------------------------------------------------------------------------\n";

    auto print_phase = [&](const std::string& name, double w, double b) {
        std::ostringstream w_oss, b_oss;
        if (w > 0.0) w_oss << std::fixed << std::setprecision(1) << w << "%";
        else w_oss << "-";
        if (b > 0.0) b_oss << std::fixed << std::setprecision(1) << b << "%";
        else b_oss << "-";

        os << " " << std::left << std::setw(42) << name
           << std::setw(19) << w_oss.str()
           << b_oss.str() << "\n";
    };
    print_phase("Opening", review.white_stats.opening_accuracy, review.black_stats.opening_accuracy);
    print_phase("Middlegame", review.white_stats.middlegame_accuracy, review.black_stats.middlegame_accuracy);
    if (review.white_stats.endgame_accuracy > 0.0 || review.black_stats.endgame_accuracy > 0.0) {
        print_phase("Endgame", review.white_stats.endgame_accuracy, review.black_stats.endgame_accuracy);
    }
    os << "--------------------------------------------------------------------------------\n";
    os << " Move Classification                       White              Black\n";
    os << "--------------------------------------------------------------------------------\n";

    auto print_row = [&](const std::string& symbol, const std::string& name, int w, int b) {
        std::string label = symbol + " " + name;
        os << " " << std::left << std::setw(42) << label
           << std::setw(19) << w
           << b << "\n";
    };

    print_row("!!", "Brilliant Moves", review.white_stats.brilliant_count, review.black_stats.brilliant_count);
    print_row("!", "Great Moves", review.white_stats.great_count, review.black_stats.great_count);
    print_row("[*]", "Best Moves", review.white_stats.best_count, review.black_stats.best_count);
    print_row("[!]", "Excellent Moves", review.white_stats.excellent_count, review.black_stats.excellent_count);
    print_row("[+]", "Good Moves", review.white_stats.good_count, review.black_stats.good_count);
    print_row("[B]", "Book Moves", review.white_stats.book_count, review.black_stats.book_count);
    print_row("[?!]", "Inaccuracies", review.white_stats.inaccuracy_count, review.black_stats.inaccuracy_count);
    print_row("[?]", "Mistakes", review.white_stats.mistake_count, review.black_stats.mistake_count);
    print_row("[??]", "Blunders", review.white_stats.blunder_count, review.black_stats.blunder_count);
    print_row("[X]", "Missed Wins", review.white_stats.miss_count, review.black_stats.miss_count);
    print_row("[]", "Forced Moves", review.white_stats.forced_count, review.black_stats.forced_count);

    os << "================================================================================\n";

    // Print critical mistakes / blunders / misses
    bool has_critical = false;
    for (size_t i = 0; i < review.plies.size(); ++i) {
        const auto& ap = review.plies[i];
        if (ap.classification == MoveClassification::Blunder ||
            ap.classification == MoveClassification::Mistake ||
            ap.classification == MoveClassification::Miss) {
            if (!has_critical) {
                os << " Key Mistakes & Blunders:\n";
                has_critical = true;
            }
            int move_num = static_cast<int>(i / 2) + 1;
            std::string prefix = (i % 2 == 0) ? (std::to_string(move_num) + ". ")
                                              : (std::to_string(move_num) + "... ");
            std::string best_display = !ap.best_was_san.empty() ? ap.best_was_san : ap.before_eval.best_move;
            os << "   " << std::left << std::setw(12) << (prefix + ap.ply.san)
               << std::setw(16) << ("(" + to_string(ap.classification) + ")")
               << " Loss: -" << std::setw(3) << static_cast<int>(ap.win_percent_loss * 100.0) << "%"
               << " | Best was: " << best_display << "\n";
        }
    }
    if (has_critical) {
        os << "================================================================================\n";
    }
    os << "\n";
}

} // namespace zugfish
