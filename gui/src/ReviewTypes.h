// Plain data types shared by the UI layer and the backends.
// They deliberately know nothing about Qt Quick, chess.hpp or the zugfish core:
// backends translate their own types into these.
#pragma once

#include <QMap>
#include <QString>
#include <QVector>
#include <functional>

namespace zq {

// Order matters: it is the index used by the QML side (Theme.classColors etc.).
enum MoveClass {
    Brilliant = 0, Great, Best, Excellent, Good, Book,
    Inaccuracy, Mistake, Blunder, Miss, Forced,
    ClassCount
};

struct PlyData {
    QString san;        // "Nf3"
    QString uci;        // "g1f3" (castling as e1g1)
    QString fenBefore;  // position this move was played from
};

struct GameData {
    QMap<QString, QString> headers;   // PGN tags: White, Black, WhiteElo, Result, Opening, ...
    QVector<PlyData> plies;
    QString finalFen;                 // position after the last ply
};

// Engine verdict for one position. Always from White's point of view.
struct EvalData {
    int value = 0;            // centipawns, or moves-to-mate when isMate (sign = who is winning)
    bool isMate = false;
    double winWhite = 50.0;   // 0..100
    QString bestUci, bestSan;
};

struct LearningPlyData {
    QString san;
    QString uci;
    QString fenBefore;
    QString fenAfter;
    EvalData after;
};

struct AnalyzedData {
    int cls = Good;
    EvalData before;                      // evaluation before this ply
    EvalData after;                       // evaluation after this ply
    QString bestBeforeUci, bestBeforeSan; // what the engine preferred instead
    QString nextBestUci, nextBestSan;     // engine's suggestion for the following move
    double winLoss = 0.0;                 // 0..1, win probability the mover gave up
    int cpLoss = 0;
    QVector<LearningPlyData> bestLine;    // engine's best move + best replies, up to 20 plies (10 moves per side)
    QVector<LearningPlyData> playedLine;  // actual move + best replies, up to 20 plies (10 moves per side)
};

struct ReviewData {
    EvalData initial;                     // start position
    QVector<AnalyzedData> plies;          // one per ply
    double whiteAccuracy = 0.0;           // 0..100
    double blackAccuracy = 0.0;
    double whiteOpeningAccuracy = 0.0;
    double blackOpeningAccuracy = 0.0;
    double whiteMiddlegameAccuracy = 0.0;
    double blackMiddlegameAccuracy = 0.0;
    double whiteEndgameAccuracy = 0.0;
    double blackEndgameAccuracy = 0.0;
    int whiteRating = 0;                  // estimated performance rating
    int blackRating = 0;
    int counts[2][ClassCount] = {};       // [0] = White, [1] = Black
};

struct OnlineGame {
    QString date, white, black, speed, result, pgn;
    int whiteRating = 0, blackRating = 0;
};

enum class Platform { ChessCom = 0, Lichess = 1 };

struct ParseResult {
    bool ok = false;
    GameData game;
    QString error;
};

using ProgressFn = std::function<void(int current, int total, double whiteAcc, double blackAcc)>;
using CancelFn = std::function<bool()>;

} // namespace zq
