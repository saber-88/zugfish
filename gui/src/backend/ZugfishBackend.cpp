// Written against the zugfish API as used by the earlier ImGui front end:
//   zugfish::parse_pgn(std::string) -> {success, game, error}
//   zugfish::GameReviewer(path).review(game, depth, onProgress, isCancelled) -> GameReview
//   zugfish::OnlineImporter::fetch_recent_games(platform, user, max, &err)
// If a name differs in your headers, the compiler will point at the exact line here.
#include "ZugfishBackend.h"

#include <chess.hpp>
#include <cmath>
#include <string>

#include "zugfish.hpp"

namespace zq {
namespace {

QString q(const std::string &s) { return QString::fromStdString(s); }

int mapClass(zugfish::MoveClassification c)
{
    using C = zugfish::MoveClassification;
    switch (c) {
    case C::Brilliant:  return Brilliant;
    case C::Great:      return Great;
    case C::Best:       return Best;
    case C::Excellent:  return Excellent;
    case C::Good:       return Good;
    case C::Book:       return Book;
    case C::Inaccuracy: return Inaccuracy;
    case C::Mistake:    return Mistake;
    case C::Blunder:    return Blunder;
    case C::Miss:       return Miss;
    case C::Forced:     return Forced;
    }
    return Good;
}

// Same curve the core uses; only needed if the core did not report accuracy itself.
double accuracyFromLoss(double winLoss01)
{
    const double pct = winLoss01 * 100.0;
    return qBound(0.0, 103.1668 * std::exp(-0.04354 * pct) - 3.1669, 100.0);
}

void addCount(ReviewData &r, bool white, int cls, int n) { r.counts[white ? 0 : 1][cls] += n; }

} // namespace

ParseResult ZugfishBackend::parsePgn(const QString &pgnText)
{
    ParseResult out;
    const auto res = zugfish::parse_pgn(pgnText.toStdString());
    if (!res.success) {
        out.error = q(res.error);
        return out;
    }
    for (const auto &kv : res.game.headers)
        out.game.headers.insert(q(kv.first), q(kv.second));
    for (const auto &p : res.game.plies)
        out.game.plies.push_back({q(p.san), q(p.uci), q(p.fen_before)});

    // position after the last move (the core stores positions *before* each move)
    if (!res.game.plies.empty()) {
        try {
            chess::Board board(res.game.plies.back().fen_before);
            board.makeMove(chess::uci::uciToMove(board, res.game.plies.back().uci));
            out.game.finalFen = q(board.getFen());
        } catch (...) {
            out.game.finalFen = out.game.plies.last().fenBefore;
        }
    }
    out.ok = true;
    return out;
}

bool ZugfishBackend::review(const GameData &game, int depth, const QString &enginePath,
                            const ProgressFn &progress, const CancelFn &cancelled,
                            ReviewData &out, QString &error)
{
    if (enginePath.isEmpty()) {
        error = QStringLiteral("Stockfish was not found. Set its path in Settings.");
        return false;
    }

    // rebuild the core's Game from the UI's plain data
    zugfish::Game g;
    for (auto it = game.headers.cbegin(); it != game.headers.cend(); ++it)
        g.headers[it.key().toStdString()] = it.value().toStdString();
    for (const PlyData &p : game.plies) {
        zugfish::Ply ply;
        ply.san = p.san.toStdString();
        ply.uci = p.uci.toStdString();
        ply.fen_before = p.fenBefore.toStdString();
        g.plies.push_back(std::move(ply));
    }

    double lastWhiteAcc = -1.0, lastBlackAcc = -1.0;
    const int total = game.plies.size();

    zugfish::GameReviewer reviewer(enginePath.toStdString());
    const auto rev = reviewer.review(
        g, depth,
        [&](const zugfish::ReviewProgress &p) {
            lastWhiteAcc = p.current_accuracy_white;
            lastBlackAcc = p.current_accuracy_black;
            progress(static_cast<int>(p.current_ply), total, lastWhiteAcc, lastBlackAcc);
        },
        [&] { return cancelled(); });

    if (cancelled())
        return false;

    // initial position
    {
        const bool whiteToMove = true;     // games here always start from the standard position
        const auto score = rev.initial_eval.score.to_white_perspective(whiteToMove);
        out.initial.value = score.value;
        out.initial.isMate = score.is_mate;
        out.initial.winWhite = rev.initial_eval.score.win_probability(whiteToMove) * 100.0;
        out.initial.bestUci = q(rev.initial_eval.best_move);
        out.initial.bestSan = q(rev.initial_best_san);
    }

    double sumW = 0, sumB = 0;
    int nW = 0, nB = 0;
    for (size_t i = 0; i < rev.plies.size(); ++i) {
        const auto &a = rev.plies[i];
        const bool white = (i % 2 == 0);
        AnalyzedData d;
        d.cls = mapClass(a.classification);
        const bool whiteToMoveBefore = (a.ply.fen_before.find(" w ") != std::string::npos);
        const auto beforeScore = a.before_eval.score.to_white_perspective(whiteToMoveBefore);
        d.before.value = beforeScore.value;
        d.before.isMate = beforeScore.is_mate;
        d.before.winWhite = a.before_eval.score.win_probability(whiteToMoveBefore) * 100.0;
        d.before.bestUci = q(a.before_eval.best_move);
        d.before.bestSan = q(a.best_was_san);
        d.cls = mapClass(a.classification);
        d.after.value = a.score_white_perspective.value;
        d.after.isMate = a.score_white_perspective.is_mate;
        d.after.winWhite = a.win_percent_white;
        d.bestBeforeUci = q(a.before_eval.best_move);
        d.bestBeforeSan = q(a.best_was_san);
        d.nextBestUci = q(a.after_eval.best_move);
        d.nextBestSan = q(a.next_best_san);
        d.after.bestUci = d.nextBestUci;
        d.after.bestSan = d.nextBestSan;
        d.winLoss = a.win_percent_loss;
        d.cpLoss = a.centipawn_loss;
        auto copyLine = [](const std::vector<zugfish::LearningPly> &source, QVector<LearningPlyData> &target) {
            target.reserve(static_cast<qsizetype>(source.size()));
            for (const auto &p : source) {
                LearningPlyData item;
                item.san = q(p.ply.san);
                item.uci = q(p.ply.uci);
                item.fenBefore = q(p.ply.fen_before);
                item.fenAfter = q(p.fen_after);
                item.after.value = p.score_white_perspective.value;
                item.after.isMate = p.score_white_perspective.is_mate;
                item.after.winWhite = p.win_percent_white;
                item.after.bestUci = q(p.best_uci);
                target.push_back(std::move(item));
            }
        };
        copyLine(a.best_line, d.bestLine);
        copyLine(a.played_line, d.playedLine);
        out.plies.push_back(d);

        addCount(out, white, d.cls, 1);
        (white ? sumW : sumB) += accuracyFromLoss(d.winLoss);
        ++(white ? nW : nB);
    }

    // prefer the accuracy the core itself reported; otherwise derive it
    out.whiteAccuracy = lastWhiteAcc >= 0 ? lastWhiteAcc : (nW ? sumW / nW : 0.0);
    out.blackAccuracy = lastBlackAcc >= 0 ? lastBlackAcc : (nB ? sumB / nB : 0.0);
    // Pass calibrated accuracies and phase breakdown from zugfish core
    out.whiteAccuracy = rev.white_stats.accuracy;
    out.blackAccuracy = rev.black_stats.accuracy;
    out.whiteOpeningAccuracy = rev.white_stats.opening_accuracy;
    out.blackOpeningAccuracy = rev.black_stats.opening_accuracy;
    out.whiteMiddlegameAccuracy = rev.white_stats.middlegame_accuracy;
    out.blackMiddlegameAccuracy = rev.black_stats.middlegame_accuracy;
    out.whiteEndgameAccuracy = rev.white_stats.endgame_accuracy;
    out.blackEndgameAccuracy = rev.black_stats.endgame_accuracy;
    out.whiteRating = rev.white_stats.estimated_rating;
    out.blackRating = rev.black_stats.estimated_rating;
    return true;
}

QVector<OnlineGame> ZugfishBackend::fetchGames(Platform platform, const QString &user,
                                               int maxGames, QString &error)
{
    std::string err;
    const auto games = zugfish::OnlineImporter::fetch_recent_games(
        platform == Platform::Lichess ? zugfish::OnlinePlatform::Lichess : zugfish::OnlinePlatform::ChessCom,
        user.toStdString(), maxGames, &err);
    error = q(err);

    QVector<OnlineGame> out;
    for (const auto &g : games) {
        OnlineGame o;
        o.date = q(g.date);
        o.white = q(g.white_name);
        o.black = q(g.black_name);
        o.whiteRating = g.white_rating;
        o.blackRating = g.black_rating;
        o.speed = q(g.speed);
        o.result = q(g.result);
        o.pgn = q(g.pgn);
        out.push_back(o);
    }
    return out;
}

} // namespace zq
