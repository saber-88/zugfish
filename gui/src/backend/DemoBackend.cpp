#include "DemoBackend.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QThread>

namespace zq {

static EvalData evalFromJson(const QJsonObject &o)
{
    EvalData e;
    e.value = o.value("scoreValue").toInt();
    e.isMate = o.value("isMate").toBool();
    e.winWhite = o.value("winWhite").toDouble(50.0);
    e.bestUci = o.value("bestUci").toString();
    e.bestSan = o.value("bestSan").toString();
    return e;
}

DemoBackend::DemoBackend()
{
    QFile f(QStringLiteral(":/data/demo_games.json"));
    if (!f.open(QIODevice::ReadOnly))
        return;
    const QJsonArray games = QJsonDocument::fromJson(f.readAll()).array();
    for (const QJsonValue &gv : games) {
        const QJsonObject g = gv.toObject();
        Entry e;
        e.id = g.value("id").toString();

        const QJsonObject headers = g.value("headers").toObject();
        for (auto it = headers.begin(); it != headers.end(); ++it)
            e.game.headers.insert(it.key(), it.value().toString());
        for (const QJsonValue &pv : g.value("plies").toArray()) {
            const QJsonObject p = pv.toObject();
            e.game.plies.push_back({p.value("san").toString(), p.value("uci").toString(),
                                    p.value("fenBefore").toString()});
        }
        e.game.finalFen = g.value("finalFen").toString();

        const QJsonObject r = g.value("review").toObject();
        e.review.initial = evalFromJson(r.value("initial").toObject());
        for (const QJsonValue &av : r.value("plies").toArray()) {
            const QJsonObject a = av.toObject();
            AnalyzedData d;
            d.cls = a.value("cls").toInt();
            d.after.value = a.value("scoreValue").toInt();
            d.after.isMate = a.value("isMate").toBool();
            d.after.winWhite = a.value("winWhite").toDouble(50.0);
            d.after.bestUci = a.value("nextBestUci").toString();
            d.after.bestSan = a.value("nextBestSan").toString();
            d.bestBeforeUci = a.value("bestBeforeUci").toString();
            d.bestBeforeSan = a.value("bestBeforeSan").toString();
            d.nextBestUci = d.after.bestUci;
            d.nextBestSan = d.after.bestSan;
            d.winLoss = a.value("winLoss").toDouble();
            d.cpLoss = a.value("cpLoss").toInt();
            e.review.plies.push_back(d);
        }
        e.review.whiteAccuracy = r.value("whiteAccuracy").toDouble();
        e.review.blackAccuracy = r.value("blackAccuracy").toDouble();
        const QJsonObject counts = r.value("counts").toObject();
        static const char *names[ClassCount] = {"Brilliant", "Great", "Best", "Excellent", "Good", "Book",
                                                "Inaccuracy", "Mistake", "Blunder", "Miss", "Forced"};
        for (int side = 0; side < 2; ++side) {
            const QJsonObject c = counts.value(side == 0 ? "white" : "black").toObject();
            for (int i = 0; i < ClassCount; ++i)
                e.review.counts[side][i] = c.value(QLatin1String(names[i])).toInt();
        }
        entries_.push_back(std::move(e));
    }

    QFile s(QStringLiteral(":/data/samples.json"));
    if (s.open(QIODevice::ReadOnly)) {
        const QJsonArray arr = QJsonDocument::fromJson(s.readAll()).array();
        for (const QJsonValue &sv : arr)
            for (Entry &e : entries_)
                if (e.id == sv.toObject().value("id").toString())
                    e.pgn = sv.toObject().value("pgn").toString();
    }
}

const DemoBackend::Entry *DemoBackend::find(const QMap<QString, QString> &headers) const
{
    for (const Entry &e : entries_)
        if (e.game.headers.value("Event") == headers.value("Event")
            && e.game.headers.value("White") == headers.value("White"))
            return &e;
    return nullptr;
}

ParseResult DemoBackend::parsePgn(const QString &text)
{
    ParseResult r;
    for (const Entry &e : entries_) {
        const QString event = e.game.headers.value("Event");
        if (text.contains(QStringLiteral("[Event \"%1\"]").arg(event))) {
            r.ok = true;
            r.game = e.game;
            return r;
        }
    }
    r.error = QStringLiteral("The demo backend can only open the bundled sample games. "
                             "Build with -DZUGFISH_CORE_DIR=... to parse any PGN.");
    return r;
}

bool DemoBackend::review(const GameData &game, int, const QString &, const ProgressFn &progress,
                         const CancelFn &cancelled, ReviewData &out, QString &error)
{
    const Entry *e = find(game.headers);
    if (!e) {
        error = QStringLiteral("No demo review is bundled for this game.");
        return false;
    }
    const int total = e->game.plies.size();
    for (int i = 1; i <= total; ++i) {          // pretend to think, so the progress UI is exercised
        if (cancelled())
            return false;
        QThread::msleep(45);
        progress(i, total, e->review.whiteAccuracy, e->review.blackAccuracy);
    }
    out = e->review;
    return true;
}

QVector<OnlineGame> DemoBackend::fetchGames(Platform, const QString &user, int maxGames, QString &error)
{
    QThread::msleep(450);
    QVector<OnlineGame> list;
    for (const Entry &e : entries_) {
        if (list.size() >= maxGames)
            break;
        OnlineGame g;
        g.date = e.game.headers.value("Date").left(10).replace('?', '0');
        g.white = e.game.headers.value("White");
        g.black = e.game.headers.value("Black");
        g.result = e.game.headers.value("Result");
        g.speed = QStringLiteral("classical");
        g.pgn = e.pgn;
        list.push_back(g);
    }
    if (user.trimmed().isEmpty())
        error = QStringLiteral("Enter a username.");
    return list;
}

} // namespace zq
