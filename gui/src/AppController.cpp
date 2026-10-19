#include "AppController.h"

#include <QClipboard>
#include <QFile>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include <QSet>
#include <QStandardPaths>
#include <algorithm>

using namespace zq;

namespace {

const char *kClassNames[ClassCount] = {"Brilliant", "Great", "Best", "Excellent", "Good", "Book",
                                       "Inaccuracy", "Mistake", "Blunder", "Missed win", "Forced"};

QString formatEval(int value, bool isMate, double winWhite)
{
    if (isMate) {
        if (value == 0)                                   // checkmate is on the board
            return winWhite > 50.0 ? QStringLiteral("1-0") : QStringLiteral("0-1");
        return QStringLiteral("%1M%2").arg(value < 0 ? QStringLiteral("-") : QString()).arg(qAbs(value));
    }
    const double pawns = value / 100.0;
    if (qAbs(pawns) < 0.05)
        return QStringLiteral("0.0");
    return QString::asprintf("%+.1f", pawns);
}

QString coachComment(int cls)
{
    switch (cls) {
    case Brilliant:  return QStringLiteral("A daring sacrifice that the engine confirms. The position stays in your favour.");
    case Great:      return QStringLiteral("The key move. It holds or turns the position when the alternatives do not.");
    case Best:       return QStringLiteral("The engine's first choice.");
    case Excellent:  return QStringLiteral("Almost as good as the best move. Next to nothing is lost.");
    case Good:       return QStringLiteral("A sensible move that keeps the position under control.");
    case Book:       return QStringLiteral("A well-known opening move.");
    case Inaccuracy: return QStringLiteral("A small slip. A more precise move was available.");
    case Mistake:    return QStringLiteral("This gives away a real part of the advantage.");
    case Blunder:    return QStringLiteral("A serious error that swings the evaluation sharply.");
    case Miss:       return QStringLiteral("A winning chance slipped away here.");
    case Forced:     return QStringLiteral("The only legal move.");
    }
    return {};
}

int squareFromUci(const QString &uci, int offset)
{
    if (uci.size() < offset + 2)
        return -1;
    const int f = uci[offset].unicode() - 'a';
    const int r = uci[offset + 1].unicode() - '1';
    return (f < 0 || f > 7 || r < 0 || r > 7) ? -1 : r * 8 + f;
}

QStringList squaresFromFen(const QString &fen, bool *whiteToMove)
{
    QStringList sq;
    sq.reserve(64);
    for (int i = 0; i < 64; ++i)
        sq << QString();
    const QStringList parts = fen.split(' ');
    int rank = 7, file = 0;
    for (const QChar c : parts.value(0)) {
        if (c == '/') {
            --rank;
            file = 0;
        } else if (c.isDigit()) {
            file += c.digitValue();
        } else if (rank >= 0 && file < 8) {
            sq[rank * 8 + file] = QString(c.isUpper() ? 'w' : 'b') + c.toUpper();
            ++file;
        }
    }
    if (whiteToMove)
        *whiteToMove = parts.value(1, QStringLiteral("w")) == QLatin1String("w");
    return sq;
}

QVariantMap computeMaterial(const QStringList &sq)
{
    static const QString order = QStringLiteral("PNBRQK");
    static const int value[6] = {1, 3, 3, 5, 9, 0};
    static const int start[6] = {8, 2, 2, 2, 1, 1};
    int cnt[2][6] = {};
    for (const QString &s : sq)
        if (!s.isEmpty())
            ++cnt[s[0] == 'b'][order.indexOf(s[1])];

    int pts[2] = {0, 0};
    for (int side = 0; side < 2; ++side)
        for (int t = 0; t < 6; ++t)
            pts[side] += cnt[side][t] * value[t];

    // Pieces of `victim` that are gone, largest first. Promotions are accounted for,
    // so a promoted pawn is not shown as a captured pawn.
    auto captured = [&](int victim) {
        int promoted = 0;
        for (int t = 1; t <= 4; ++t)
            promoted += qMax(0, cnt[victim][t] - start[t]);
        QStringList out;
        const QChar colour = victim == 0 ? 'w' : 'b';
        for (int t = 4; t >= 0; --t) {
            const int missing = t == 0 ? qMax(0, start[0] - cnt[victim][0] - promoted)
                                       : qMax(0, start[t] - cnt[victim][t]);
            for (int i = 0; i < missing; ++i)
                out << (QString(colour) + order[t]);
        }
        return out;
    };

    QVariantMap m;
    m["whiteCaptured"] = captured(1);   // black pieces White has taken
    m["blackCaptured"] = captured(0);
    m["diff"] = pts[0] - pts[1];        // > 0: White is ahead on material
    return m;
}

QString prettyDate(const QString &pgnDate)
{
    QStringList keep;
    for (const QString &part : pgnDate.split('.')) {
        if (part.contains('?'))
            break;
        keep << part;
    }
    return keep.join('-');
}

} // namespace

AppController::AppController(std::unique_ptr<IBackend> backend, QObject *parent)
    : QObject(parent), backend_(std::move(backend)), settings_(QStringLiteral("zugfish"), QStringLiteral("zugfish-qt"))
{
    enginePath_ = settings_.value("enginePath").toString();
    if (enginePath_.isEmpty())
        enginePath_ = QStandardPaths::findExecutable(QStringLiteral("stockfish"));
    depth_ = settings_.value("depth", 14).toInt();
    boardTheme_ = settings_.value("boardTheme", 0).toInt();
    showArrows_ = settings_.value("showArrows", true).toBool();
    animateMoves_ = settings_.value("animateMoves", true).toBool();
    autoReview_ = settings_.value("autoReview", false).toBool();

    QFile f(QStringLiteral(":/data/samples.json"));
    if (f.open(QIODevice::ReadOnly))
        samples_ = QJsonDocument::fromJson(f.readAll()).array().toVariantList();

    autoplay_.setInterval(1100);
    connect(&autoplay_, &QTimer::timeout, this, [this] {
        if (!canNext()) {
            autoplay_.stop();
            emit autoplayChanged();
        } else {
            next();
        }
    });

    rebuildPosition();
    rebuildReviewDerived();
}

AppController::~AppController()
{
    stopWorker();
    if (fetchThread_.joinable())
        fetchThread_.join();
}

// ---------------------------------------------------------------- settings

void AppController::setEnginePath(const QString &p)
{
    if (p == enginePath_) return;
    enginePath_ = p;
    settings_.setValue("enginePath", p);
    emit settingsChanged();
}
void AppController::setDepth(int d)
{
    d = qBound(6, d, 30);
    if (d == depth_) return;
    depth_ = d;
    settings_.setValue("depth", d);
    emit settingsChanged();
}
void AppController::setBoardTheme(int t)
{
    if (t == boardTheme_) return;
    boardTheme_ = t;
    settings_.setValue("boardTheme", t);
    emit settingsChanged();
}
void AppController::setShowArrows(bool v)
{
    if (v == showArrows_) return;
    showArrows_ = v;
    settings_.setValue("showArrows", v);
    emit settingsChanged();
}
void AppController::setAnimateMoves(bool v)
{
    if (v == animateMoves_) return;
    animateMoves_ = v;
    settings_.setValue("animateMoves", v);
    emit settingsChanged();
}
void AppController::setAutoReview(bool v)
{
    if (v == autoReview_) return;
    autoReview_ = v;
    settings_.setValue("autoReview", v);
    emit settingsChanged();
}

// ---------------------------------------------------------------- loading

QString AppController::loadPgn(const QString &text)
{
    if (text.trimmed().isEmpty())
        return QStringLiteral("Paste a PGN first.");
    const ParseResult r = backend_->parsePgn(text);
    if (!r.ok)
        return r.error.isEmpty() ? QStringLiteral("Could not read this PGN.") : r.error;
    return loadGame(r.game);
}

QString AppController::loadPgnFile(const QUrl &url)
{
    QFile f(url.isLocalFile() ? url.toLocalFile() : url.toString());
    if (!f.open(QIODevice::ReadOnly)) {
        const QString msg = QStringLiteral("Could not open %1").arg(f.fileName());
        emit toast(msg, Error);
        return msg;
    }
    const QString err = loadPgn(QString::fromUtf8(f.readAll()));
    if (!err.isEmpty())
        emit toast(err, Error);
    return err;
}

QString AppController::loadSample(const QString &id)
{
    for (const QVariant &v : std::as_const(samples_)) {
        const QVariantMap m = v.toMap();
        if (m.value("id").toString() == id) {
            const QString err = loadPgn(m.value("pgn").toString());
            if (!err.isEmpty())
                emit toast(err, Error);
            return err;
        }
    }
    return QStringLiteral("Unknown sample.");
}

QString AppController::loadGame(const GameData &game)
{
    if (game.plies.isEmpty())
        return QStringLiteral("This PGN has no moves.");

    stopWorker();
    autoplay_.stop();
    learningMode_ = false;
    learningMistakePly_ = 0;
    learningStep_ = 0;
    learningLine_ = 0;
    game_ = game;
    if (game_.finalFen.isEmpty())
        game_.finalFen = game_.plies.last().fenBefore;
    ply_ = 0;
    hasReview_ = false;
    state_ = Idle;
    progress_ = 0.0;
    reviewError_.clear();
    review_ = ReviewData();

    const auto h = [&](const char *k) { return game_.headers.value(QLatin1String(k)); };
    gameInfo_.clear();
    gameInfo_["white"] = h("White").isEmpty() ? QStringLiteral("White") : h("White");
    gameInfo_["black"] = h("Black").isEmpty() ? QStringLiteral("Black") : h("Black");
    gameInfo_["whiteElo"] = h("WhiteElo");
    gameInfo_["blackElo"] = h("BlackElo");
    gameInfo_["result"] = h("Result");
    gameInfo_["event"] = h("Event");
    gameInfo_["site"] = h("Site");
    gameInfo_["date"] = prettyDate(h("Date"));
    gameInfo_["opening"] = h("Opening");
    gameInfo_["timeControl"] = h("TimeControl");
    gameInfo_["termination"] = h("Termination");

    plyModel_.setGame(game_);
    rebuildReviewDerived();
    rebuildPosition();
    emit gameChanged();
    emit currentChanged();
    emit reviewChanged();
    emit autoplayChanged();

    if (autoReview_)
        startReview();
    return {};
}

void AppController::closeGame()
{
    stopWorker();
    autoplay_.stop();
    learningMode_ = false;
    learningMistakePly_ = 0;
    learningStep_ = 0;
    learningLine_ = 0;
    game_ = GameData();
    gameInfo_.clear();
    ply_ = 0;
    hasReview_ = false;
    state_ = Idle;
    plyModel_.setGame(game_);
    rebuildReviewDerived();
    rebuildPosition();
    emit gameChanged();
    emit currentChanged();
    emit reviewChanged();
    emit autoplayChanged();
}

// ---------------------------------------------------------------- navigation

QString AppController::fenAt(int ply) const
{
    if (game_.plies.isEmpty())
        return QStringLiteral("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    if (ply >= game_.plies.size())
        return game_.finalFen;
    return game_.plies[qMax(0, ply)].fenBefore;
}

const QVector<zq::LearningPlyData> &AppController::activeLearningLine() const
{
    static const QVector<zq::LearningPlyData> empty;
    if (!learningMode_ || learningMistakePly_ < 1 || learningMistakePly_ > review_.plies.size())
        return empty;
    const zq::AnalyzedData &a = review_.plies[learningMistakePly_ - 1];
    return learningLine_ == 0 ? a.bestLine : a.playedLine;
}

int AppController::learningStepCount() const
{
    return activeLearningLine().size();
}

QString AppController::displayFen() const
{
    if (!learningMode_)
        return fenAt(ply_);
    if (learningMistakePly_ < 1 || learningMistakePly_ > game_.plies.size())
        return fenAt(ply_);
    if (learningStep_ <= 0)
        return game_.plies[learningMistakePly_ - 1].fenBefore;
    const auto &line = activeLearningLine();
    if (learningStep_ <= line.size())
        return line[learningStep_ - 1].fenAfter;
    return line.isEmpty() ? game_.plies[learningMistakePly_ - 1].fenBefore : line.last().fenAfter;
}

QString AppController::fen() const
{
    return displayFen();
}

void AppController::setPly(int ply)
{
    const bool wasLearning = learningMode_;
    learningMode_ = false;
    ply = qBound(0, ply, plyCount());
    if (ply == ply_) {
        if (wasLearning) {
            rebuildPosition();
            emit currentChanged();
        }
        return;
    }
    ply_ = ply;
    rebuildPosition();
    emit currentChanged();
}

void AppController::goTo(int ply)
{
    if (autoplay_.isActive()) {          // manual navigation always stops autoplay
        autoplay_.stop();
        emit autoplayChanged();
    }
    setPly(ply);
}

void AppController::next()
{
    if (learningMode_) {
        if (learningStep_ < learningStepCount()) {
            ++learningStep_;
            rebuildPosition();
            emit currentChanged();
        }
        return;
    }
    goTo(ply_ + 1);
}

void AppController::prev()
{
    if (learningMode_) {
        if (learningStep_ > 0) {
            --learningStep_;
            rebuildPosition();
            emit currentChanged();
        }
        return;
    }
    goTo(ply_ - 1);
}

void AppController::first()
{
    if (learningMode_) {
        learningStep_ = 0;
        rebuildPosition();
        emit currentChanged();
        return;
    }
    goTo(0);
}

void AppController::last()
{
    if (learningMode_) {
        learningStep_ = learningStepCount();
        rebuildPosition();
        emit currentChanged();
        return;
    }
    goTo(plyCount());
}

void AppController::openLearning(int mistakePly, int line)
{
    if (!hasReview_ || mistakePly < 1 || mistakePly > review_.plies.size())
        return;
    const zq::AnalyzedData &a = review_.plies[mistakePly - 1];
    if (a.bestLine.isEmpty() || a.playedLine.isEmpty())
        return;
    if (autoplay_.isActive()) {
        autoplay_.stop();
        emit autoplayChanged();
    }
    learningMistakePly_ = mistakePly;
    ply_ = mistakePly; // keep the original game's mistake selected in the graph/move list
    learningLine_ = line == 1 ? 1 : 0;
    learningMode_ = true;
    learningStep_ = 0;
    rebuildPosition();
    emit currentChanged();
}

void AppController::switchLearningLine(int line)
{
    if (!learningMode_)
        return;
    const int requested = line == 1 ? 1 : 0;
    const zq::AnalyzedData &a = review_.plies[learningMistakePly_ - 1];
    if ((requested == 0 && a.bestLine.isEmpty()) || (requested == 1 && a.playedLine.isEmpty()))
        return;
    learningLine_ = requested;
    learningStep_ = 0;
    rebuildPosition();
    emit currentChanged();
}

void AppController::closeLearning()
{
    if (!learningMode_)
        return;
    learningMode_ = false;
    learningStep_ = 0;
    rebuildPosition();
    emit currentChanged();
}

void AppController::setFlipped(bool f)
{
    if (f == flipped_) return;
    flipped_ = f;
    emit flippedChanged();
}

void AppController::toggleAutoplay()
{
    if (autoplay_.isActive()) {
        autoplay_.stop();
    } else {
        if (!canNext())
            first();
        autoplay_.start();
    }
    emit autoplayChanged();
}

void AppController::jumpKey(int direction)
{
    if (!hasReview_) return;
    static const QSet<int> key{Brilliant, Great, Mistake, Blunder, Miss};
    const int n = review_.plies.size();
    for (int p = ply_ + direction; p >= 1 && p <= n; p += direction) {
        if (key.contains(review_.plies[p - 1].cls)) {
            goTo(p);
            return;
        }
    }
}

void AppController::jumpToClass(int cls, int direction)
{
    if (!hasReview_) return;
    const int n = review_.plies.size();
    for (int step = 1; step <= n; ++step) {
        const int p = ((ply_ - 1 + direction * step) % n + n) % n + 1;   // wraps around
        if (review_.plies[p - 1].cls == cls) {
            goTo(p);
            return;
        }
    }
}

void AppController::rebuildPosition()
{
    squares_ = squaresFromFen(displayFen(), &whiteToMove_);
    material_ = computeMaterial(squares_);

    lastFrom_ = lastTo_ = bestFrom_ = bestTo_ = -1;
    if (learningMode_) {
        const auto &line = activeLearningLine();
        const zq::AnalyzedData &mistake = review_.plies[learningMistakePly_ - 1];
        if (learningStep_ > 0 && learningStep_ <= line.size()) {
            const QString &uci = line[learningStep_ - 1].uci;
            lastFrom_ = squareFromUci(uci, 0);
            lastTo_ = squareFromUci(uci, 2);
            const QString best = line[learningStep_ - 1].after.bestUci;
            if (!best.isEmpty() && best != uci) {
                bestFrom_ = squareFromUci(best, 0);
                bestTo_ = squareFromUci(best, 2);
            }
        } else if (!mistake.bestBeforeUci.isEmpty()) {
            bestFrom_ = squareFromUci(mistake.bestBeforeUci, 0);
            bestTo_ = squareFromUci(mistake.bestBeforeUci, 2);
        }
    } else if (ply_ > 0 && ply_ <= plyCount()) {
        const QString &uci = game_.plies[ply_ - 1].uci;
        lastFrom_ = squareFromUci(uci, 0);
        lastTo_ = squareFromUci(uci, 2);
        if (hasReview_ && ply_ <= review_.plies.size()) {
            const AnalyzedData &a = review_.plies[ply_ - 1];
            if (!a.bestBeforeUci.isEmpty() && a.bestBeforeUci != uci && a.cls != Book && a.cls != Forced) {
                bestFrom_ = squareFromUci(a.bestBeforeUci, 0);
                bestTo_ = squareFromUci(a.bestBeforeUci, 2);
            }
        }
    } else if (ply_ == 0 && hasReview_ && !review_.initial.bestUci.isEmpty()) {
        bestFrom_ = squareFromUci(review_.initial.bestUci, 0);
        bestTo_ = squareFromUci(review_.initial.bestUci, 2);
    }
    current_ = makeCurrent();
}

QVariantMap AppController::makeCurrent() const
{
    QVariantMap m;
    m["ply"] = ply_;
    m["has"] = false;
    m["hasEval"] = false;
    m["winWhite"] = 50.0;
    m["evalText"] = QStringLiteral("–");
    m["cls"] = -1;

    if (learningMode_ && learningMistakePly_ >= 1 && learningMistakePly_ <= review_.plies.size()) {
        const zq::AnalyzedData &mistake = review_.plies[learningMistakePly_ - 1];
        const auto &line = activeLearningLine();
        const zq::EvalData eval = learningStep_ == 0 ? mistake.before : line[learningStep_ - 1].after;
        m["has"] = true;
        m["hasEval"] = true;
        // In the recommended line, every engine-selected move is a best move.
        // In the punishment line, only the first ply is the user's original mistake;
        // subsequent plies are the opponent's best reply and the engine's best continuation.
        const int learningClass = (learningLine_ == 1 && learningStep_ == 1) ? mistake.cls : Best;
        m["cls"] = learningStep_ == 0 ? -1 : learningClass;
        m["className"] = learningStep_ == 0
            ? QStringLiteral("Compare alternatives")
            : (learningClass == Best ? QStringLiteral("Best continuation")
                                     : QStringLiteral("Your played move"));
        m["winWhite"] = eval.winWhite;
        m["isMate"] = eval.isMate;
        m["evalText"] = formatEval(eval.value, eval.isMate, eval.winWhite);
        m["bestBeforeSan"] = mistake.bestBeforeSan;
        m["bestBeforeUci"] = mistake.bestBeforeUci;
        m["isBest"] = (learningLine_ == 0 || learningStep_ > 1);
        m["winLoss"] = mistake.winLoss;
        m["cpLoss"] = mistake.cpLoss;
        m["nextBestSan"] = eval.bestSan;
        m["comment"] = learningStep_ == 0
            ? QStringLiteral("Compare the engine's best move with the move played in the game.")
            : (learningLine_ == 1 && learningStep_ == 1
                ? QStringLiteral("This is the move played in the game. Step forward to see the opponent's strongest response.")
                : QStringLiteral("Follow the engine's strongest continuation from this position."));
        m["learningMode"] = true;
        m["learningLineName"] = learningLineName();
        m["learningStep"] = learningStep_;
        m["learningStepCount"] = line.size();
        if (learningStep_ == 0) {
            const QString originalSan = game_.plies[learningMistakePly_ - 1].san;
            const bool white = (learningMistakePly_ % 2 == 1);
            m["moveLabel"] = QStringLiteral("Before %1%2 %3").arg((learningMistakePly_ + 1) / 2).arg(white ? "." : "...").arg(originalSan);
            m["ply"] = 0;
        } else {
            const auto &item = line[learningStep_ - 1];
            const QStringList fenParts = item.fenBefore.split(' ');
            const bool white = fenParts.value(1, QStringLiteral("w")) == QLatin1String("w");
            const int moveNo = fenParts.value(5, QStringLiteral("1")).toInt();
            m["san"] = item.san;
            m["uci"] = item.uci;
            m["isWhite"] = white;
            m["moveLabel"] = QStringLiteral("%1%2 %3").arg(moveNo).arg(white ? "." : "...").arg(item.san);
            m["ply"] = learningStep_;
        }
        return m;
    }

    if (ply_ > 0 && ply_ <= plyCount()) {
        const PlyData &p = game_.plies[ply_ - 1];
        const bool white = (ply_ % 2 == 1);
        m["san"] = p.san;
        m["uci"] = p.uci;
        m["isWhite"] = white;
        m["moveLabel"] = QStringLiteral("%1%2 %3").arg((ply_ + 1) / 2).arg(white ? "." : "...").arg(p.san);
        if (hasReview_ && ply_ <= review_.plies.size()) {
            const AnalyzedData &a = review_.plies[ply_ - 1];
            m["has"] = true;
            m["hasEval"] = true;
            m["cls"] = a.cls;
            m["className"] = QLatin1String(kClassNames[a.cls]);
            m["winWhite"] = a.after.winWhite;
            m["isMate"] = a.after.isMate;
            m["evalText"] = formatEval(a.after.value, a.after.isMate, a.after.winWhite);
            m["bestBeforeSan"] = a.bestBeforeSan;
            m["bestBeforeUci"] = a.bestBeforeUci;
            m["isBest"] = a.bestBeforeUci == p.uci;
            m["winLoss"] = a.winLoss;
            m["cpLoss"] = a.cpLoss;
            m["nextBestSan"] = a.nextBestSan;
            m["comment"] = coachComment(a.cls);
        }
    } else {
        m["title"] = QStringLiteral("Starting position");
        if (hasReview_) {
            m["hasEval"] = true;
            m["winWhite"] = review_.initial.winWhite;
            m["evalText"] = formatEval(review_.initial.value, review_.initial.isMate, review_.initial.winWhite);
            m["initialBestSan"] = review_.initial.bestSan;
        }
    }
    return m;
}

// ---------------------------------------------------------------- review

void AppController::stopWorker()
{
    cancel_ = true;
    ++generation_;                      // queued results from the old run are ignored
    if (worker_.joinable())
        worker_.join();
    cancel_ = false;
    if (state_ == Running) {
        state_ = Idle;
        progress_ = 0.0;
    }
}

void AppController::startReview()
{
    if (!gameLoaded() || state_ == Running)
        return;
    stopWorker();

    learningMode_ = false;
    learningMistakePly_ = 0;
    learningStep_ = 0;
    state_ = Running;
    progress_ = 0.0;
    reviewError_.clear();
    hasReview_ = false;
    rebuildReviewDerived();
    rebuildPosition();
    emit reviewChanged();
    emit progressChanged();
    emit currentChanged();

    const int gen = ++generation_;
    const GameData game = game_;
    const QString engine = enginePath_;
    const int depth = depth_;

    worker_ = std::thread([this, game, engine, depth, gen] {
        ReviewData out;
        QString err;
        bool ok = false;
        try {
            ok = backend_->review(
                game, depth, engine,
                [this, gen](int cur, int total, double, double) {
                    QMetaObject::invokeMethod(this, [this, gen, cur, total] {
                        if (gen != generation_ || state_ != Running) return;
                        progress_ = total > 0 ? double(cur) / total : 0.0;
                        emit progressChanged();
                    }, Qt::QueuedConnection);
                },
                [this] { return cancel_.load(); }, out, err);
        } catch (const std::exception &e) {
            err = QString::fromUtf8(e.what());
            ok = false;
        }
        QMetaObject::invokeMethod(this, [this, gen, ok, out = std::move(out), err]() mutable {
            if (gen != generation_) return;
            finishReview(ok, std::move(out), err);
        }, Qt::QueuedConnection);
    });
}

void AppController::cancelReview()
{
    if (state_ != Running) return;
    stopWorker();
    emit reviewChanged();
    emit progressChanged();
}

void AppController::finishReview(bool ok, ReviewData data, const QString &error)
{
    if (worker_.joinable())
        worker_.join();
    if (ok) {
        review_ = std::move(data);
        hasReview_ = true;
        state_ = Done;
        progress_ = 1.0;
        rebuildReviewDerived();
        rebuildPosition();
        emit toast(QStringLiteral("Review complete: White %1%, Black %2%")
                       .arg(review_.whiteAccuracy, 0, 'f', 1).arg(review_.blackAccuracy, 0, 'f', 1), Success);
    } else {
        state_ = Failed;
        reviewError_ = error.isEmpty() ? QStringLiteral("The review failed.") : error;
        qWarning().noquote() << "review failed:" << reviewError_;
        emit toast(reviewError_, Error);
    }
    emit reviewChanged();
    emit progressChanged();
    emit currentChanged();
}

void AppController::rebuildReviewDerived()
{
    winSeries_.clear();
    classSeries_.clear();
    keyMoments_.clear();
    learningMoments_.clear();
    summary_.clear();
    summary_["done"] = hasReview_;

    QVector<int> classes;
    QVector<QString> evals;
    if (hasReview_) {
        winSeries_ << review_.initial.winWhite;
        for (const AnalyzedData &a : std::as_const(review_.plies)) {
            winSeries_ << a.after.winWhite;
            classSeries_ << a.cls;
            classes << a.cls;
            evals << formatEval(a.after.value, a.after.isMate, a.after.winWhite);
        }

        QVariantList wc, bc;
        for (int i = 0; i < ClassCount; ++i) {
            wc << review_.counts[0][i];
            bc << review_.counts[1][i];
        }
        summary_["whiteAccuracy"] = review_.whiteAccuracy;
        summary_["blackAccuracy"] = review_.blackAccuracy;
        summary_["whiteOpeningAccuracy"] = review_.whiteOpeningAccuracy;
        summary_["blackOpeningAccuracy"] = review_.blackOpeningAccuracy;
        summary_["whiteMiddlegameAccuracy"] = review_.whiteMiddlegameAccuracy;
        summary_["blackMiddlegameAccuracy"] = review_.blackMiddlegameAccuracy;
        summary_["whiteEndgameAccuracy"] = review_.whiteEndgameAccuracy;
        summary_["blackEndgameAccuracy"] = review_.blackEndgameAccuracy;
        summary_["whiteRating"] = review_.whiteRating;
        summary_["blackRating"] = review_.blackRating;
        summary_["whiteCounts"] = wc;
        summary_["blackCounts"] = bc;

        for (int i = 0; i < review_.plies.size(); ++i) {
            const zq::AnalyzedData &a = review_.plies[i];
            if (a.winLoss <= 0.10 || a.bestLine.isEmpty() || a.playedLine.isEmpty())
                continue;
            const int ply = i + 1;
            const bool white = (ply % 2 == 1);
            QVariantMap m;
            m["ply"] = ply;
            m["cls"] = a.cls;
            m["white"] = white;
            m["label"] = QStringLiteral("%1%2 %3").arg((ply + 1) / 2).arg(white ? "." : "...").arg(game_.plies[i].san);
            m["detail"] = QStringLiteral("−%1 percentage points").arg(qRound(a.winLoss * 100.0));
            m["bestMove"] = a.bestBeforeSan;
            m["bestPlies"] = a.bestLine.size();
            m["playedPlies"] = a.playedLine.size();
            learningMoments_ << m;
        }

        struct Moment { double weight; QVariantMap map; };
        QVector<Moment> moments;
        for (int i = 0; i < review_.plies.size(); ++i) {
            const AnalyzedData &a = review_.plies[i];
            const bool bad = a.cls == Mistake || a.cls == Blunder || a.cls == Miss;
            const bool good = a.cls == Brilliant || a.cls == Great;
            if (!bad && !good) continue;
            const int ply = i + 1;
            const bool white = ply % 2 == 1;
            QVariantMap m;
            m["ply"] = ply;
            m["cls"] = a.cls;
            m["white"] = white;
            m["label"] = QStringLiteral("%1%2 %3").arg((ply + 1) / 2).arg(white ? "." : "...").arg(game_.plies[i].san);
            m["detail"] = bad ? QStringLiteral("-%1% win chance").arg(qRound(a.winLoss * 100))
                              : QString::fromLatin1(kClassNames[a.cls]);
            moments.push_back({bad ? a.winLoss : 0.12 + (a.cls == Brilliant ? 0.1 : 0.0), m});
        }
        std::stable_sort(moments.begin(), moments.end(), [](const Moment &a, const Moment &b) { return a.weight > b.weight; });
        if (moments.size() > 6) moments.resize(6);
        std::stable_sort(moments.begin(), moments.end(), [](const Moment &a, const Moment &b) {
            return a.map.value("ply").toInt() < b.map.value("ply").toInt();
        });
        for (const Moment &m : std::as_const(moments))
            keyMoments_ << m.map;
    }
    plyModel_.setReview(classes, evals);
}

// ---------------------------------------------------------------- online import

void AppController::fetchOnline(int platform, const QString &user, int maxGames)
{
    if (onlineBusy_) return;
    const QString name = user.trimmed();
    if (name.isEmpty()) {
        onlineError_ = true;
        onlineStatus_ = QStringLiteral("Enter a username first.");
        emit onlineChanged();
        return;
    }
    if (fetchThread_.joinable())
        fetchThread_.join();
    onlineBusy_ = true;
    onlineError_ = false;
    onlineStatus_ = QStringLiteral("Fetching games for %1…").arg(name);
    emit onlineChanged();

    fetchThread_ = std::thread([this, platform, name, maxGames] {
        QString err;
        QVector<OnlineGame> games;
        try {
            games = backend_->fetchGames(platform == 1 ? Platform::Lichess : Platform::ChessCom, name, maxGames, err);
        } catch (const std::exception &e) {
            err = QString::fromUtf8(e.what());
        }
        QMetaObject::invokeMethod(this, [this, games = std::move(games), err, name]() mutable {
            onlineBusy_ = false;
            onlineModel_.setGames(games);
            if (games.isEmpty()) {
                onlineError_ = true;
                onlineStatus_ = err.isEmpty() ? QStringLiteral("No games found for %1.").arg(name) : err;
            } else {
                onlineError_ = false;
                onlineStatus_ = QStringLiteral("%1 games found").arg(games.size());
            }
            emit onlineChanged();
        }, Qt::QueuedConnection);
    });
}

QString AppController::loadOnlineGame(int row)
{
    const OnlineGame *g = onlineModel_.at(row);
    if (!g)
        return QStringLiteral("Select a game first.");
    const QString err = loadPgn(g->pgn);
    if (!err.isEmpty())
        emit toast(err, Error);
    return err;
}

void AppController::copyToClipboard(const QString &text)
{
    QGuiApplication::clipboard()->setText(text);
    emit toast(QStringLiteral("Copied to clipboard"), Info);
}
