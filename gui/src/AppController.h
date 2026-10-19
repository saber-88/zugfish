// The single object QML talks to (exposed as `App`).
// It owns the loaded game and its review, navigation state, settings and worker
// threads. All chess/engine work is delegated to an IBackend.
#pragma once

#include <QObject>
#include <QSettings>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <atomic>
#include <memory>
#include <thread>

#include "OnlineGamesModel.h"
#include "PlyModel.h"
#include "ReviewTypes.h"
#include "backend/IBackend.h"

class AppController : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString backendName READ backendName CONSTANT)
    Q_PROPERTY(QVariantList samples READ samples CONSTANT)

    // --- game & navigation ---
    Q_PROPERTY(bool gameLoaded READ gameLoaded NOTIFY gameChanged)
    Q_PROPERTY(QVariantMap gameInfo READ gameInfo NOTIFY gameChanged)
    Q_PROPERTY(int plyCount READ plyCount NOTIFY gameChanged)
    Q_PROPERTY(int currentPly READ currentPly NOTIFY currentChanged)
    Q_PROPERTY(bool flipped READ flipped WRITE setFlipped NOTIFY flippedChanged)
    Q_PROPERTY(bool autoplay READ autoplay NOTIFY autoplayChanged)

    // --- current position (everything the board needs) ---
    Q_PROPERTY(QString fen READ fen NOTIFY currentChanged)
    Q_PROPERTY(QStringList squares READ squares NOTIFY currentChanged)   // 64 entries: "wK", "bP", "" ; index = rank*8 + file, a1 = 0
    Q_PROPERTY(bool whiteToMove READ whiteToMove NOTIFY currentChanged)
    Q_PROPERTY(int lastFrom READ lastFrom NOTIFY currentChanged)
    Q_PROPERTY(int lastTo READ lastTo NOTIFY currentChanged)
    Q_PROPERTY(int bestFrom READ bestFrom NOTIFY currentChanged)
    Q_PROPERTY(int bestTo READ bestTo NOTIFY currentChanged)
    Q_PROPERTY(QVariantMap current READ current NOTIFY currentChanged)
    Q_PROPERTY(QVariantMap material READ material NOTIFY currentChanged)

    // --- review ---
    Q_PROPERTY(int reviewState READ reviewState NOTIFY reviewChanged)   // ReviewState
    Q_PROPERTY(double reviewProgress READ reviewProgress NOTIFY progressChanged)
    Q_PROPERTY(QString reviewError READ reviewError NOTIFY reviewChanged)
    Q_PROPERTY(QVariantMap summary READ summary NOTIFY reviewChanged)
    Q_PROPERTY(QVariantList winSeries READ winSeries NOTIFY reviewChanged)       // plyCount+1 values, White win% 0..100
    Q_PROPERTY(QVariantList classSeries READ classSeries NOTIFY reviewChanged)   // plyCount move classes
    Q_PROPERTY(QVariantList keyMoments READ keyMoments NOTIFY reviewChanged)
    Q_PROPERTY(QVariantList learningMoments READ learningMoments NOTIFY reviewChanged)
    Q_PROPERTY(bool learningMode READ learningMode NOTIFY currentChanged)
    Q_PROPERTY(int learningLine READ learningLine NOTIFY currentChanged) // 0 = best, 1 = played
    Q_PROPERTY(int learningStep READ learningStep NOTIFY currentChanged)
    Q_PROPERTY(int learningStepCount READ learningStepCount NOTIFY currentChanged)
    Q_PROPERTY(QString learningLineName READ learningLineName NOTIFY currentChanged)
    Q_PROPERTY(bool canPrev READ canPrev NOTIFY currentChanged)
    Q_PROPERTY(bool canNext READ canNext NOTIFY currentChanged)
    Q_PROPERTY(int navigationPly READ navigationPly NOTIFY currentChanged)
    Q_PROPERTY(int navigationCount READ navigationCount NOTIFY currentChanged)
    Q_PROPERTY(QAbstractItemModel *plies READ plies CONSTANT)

    // --- online import ---
    Q_PROPERTY(QAbstractItemModel *onlineGames READ onlineGames CONSTANT)
    Q_PROPERTY(bool onlineBusy READ onlineBusy NOTIFY onlineChanged)
    Q_PROPERTY(QString onlineStatus READ onlineStatus NOTIFY onlineChanged)
    Q_PROPERTY(bool onlineError READ onlineError NOTIFY onlineChanged)

    // --- settings (persisted) ---
    Q_PROPERTY(QString enginePath READ enginePath WRITE setEnginePath NOTIFY settingsChanged)
    Q_PROPERTY(int depth READ depth WRITE setDepth NOTIFY settingsChanged)
    Q_PROPERTY(int boardTheme READ boardTheme WRITE setBoardTheme NOTIFY settingsChanged)
    Q_PROPERTY(bool showArrows READ showArrows WRITE setShowArrows NOTIFY settingsChanged)
    Q_PROPERTY(bool animateMoves READ animateMoves WRITE setAnimateMoves NOTIFY settingsChanged)
    Q_PROPERTY(bool autoReview READ autoReview WRITE setAutoReview NOTIFY settingsChanged)

public:
    enum ReviewState { Idle = 0, Running, Done, Failed };
    Q_ENUM(ReviewState)
    enum ToastKind { Info = 0, Success, Error };
    Q_ENUM(ToastKind)

    explicit AppController(std::unique_ptr<zq::IBackend> backend, QObject *parent = nullptr);
    ~AppController() override;

    QString backendName() const { return backend_->name(); }
    QVariantList samples() const { return samples_; }

    bool gameLoaded() const { return !game_.plies.isEmpty(); }
    QVariantMap gameInfo() const { return gameInfo_; }
    int plyCount() const { return game_.plies.size(); }
    int currentPly() const { return ply_; }
    bool flipped() const { return flipped_; }
    void setFlipped(bool f);
    bool autoplay() const { return autoplay_.isActive(); }

    QString fen() const;
    QStringList squares() const { return squares_; }
    bool whiteToMove() const { return whiteToMove_; }
    int lastFrom() const { return lastFrom_; }
    int lastTo() const { return lastTo_; }
    int bestFrom() const { return bestFrom_; }
    int bestTo() const { return bestTo_; }
    QVariantMap current() const { return current_; }
    QVariantMap material() const { return material_; }

    int reviewState() const { return state_; }
    double reviewProgress() const { return progress_; }
    QString reviewError() const { return reviewError_; }
    QVariantMap summary() const { return summary_; }
    QVariantList winSeries() const { return winSeries_; }
    QVariantList classSeries() const { return classSeries_; }
    QVariantList keyMoments() const { return keyMoments_; }
    QVariantList learningMoments() const { return learningMoments_; }
    bool learningMode() const { return learningMode_; }
    int learningLine() const { return learningLine_; }
    int learningStep() const { return learningStep_; }
    int learningStepCount() const;
    QString learningLineName() const { return learningLine_ == 0 ? QStringLiteral("Best continuation") : QStringLiteral("Opponent's punishment"); }
    bool canPrev() const { return learningMode_ ? learningStep_ > 0 : ply_ > 0; }
    bool canNext() const { return learningMode_ ? learningStep_ < learningStepCount() : ply_ < plyCount(); }
    int navigationPly() const { return learningMode_ ? learningStep_ : ply_; }
    int navigationCount() const { return learningMode_ ? learningStepCount() : plyCount(); }
    QAbstractItemModel *plies() { return &plyModel_; }

    QAbstractItemModel *onlineGames() { return &onlineModel_; }
    bool onlineBusy() const { return onlineBusy_; }
    QString onlineStatus() const { return onlineStatus_; }
    bool onlineError() const { return onlineError_; }

    QString enginePath() const { return enginePath_; }
    void setEnginePath(const QString &p);
    int depth() const { return depth_; }
    void setDepth(int d);
    int boardTheme() const { return boardTheme_; }
    void setBoardTheme(int t);
    bool showArrows() const { return showArrows_; }
    void setShowArrows(bool v);
    bool animateMoves() const { return animateMoves_; }
    void setAnimateMoves(bool v);
    bool autoReview() const { return autoReview_; }
    void setAutoReview(bool v);

    // ---- actions callable from QML ----
    Q_INVOKABLE QString loadPgn(const QString &text);               // returns "" on success, else an error message
    Q_INVOKABLE QString loadPgnFile(const QUrl &url);
    Q_INVOKABLE QString loadSample(const QString &id);
    Q_INVOKABLE void closeGame();

    Q_INVOKABLE void startReview();
    Q_INVOKABLE void cancelReview();

    Q_INVOKABLE void goTo(int ply);
    Q_INVOKABLE void next();
    Q_INVOKABLE void prev();
    Q_INVOKABLE void first();
    Q_INVOKABLE void last();
    Q_INVOKABLE void openLearning(int mistakePly, int line = 0);
    Q_INVOKABLE void switchLearningLine(int line);
    Q_INVOKABLE void closeLearning();
    Q_INVOKABLE void jumpKey(int direction);                        // next/previous notable move
    Q_INVOKABLE void jumpToClass(int cls, int direction = 1);
    Q_INVOKABLE void toggleFlip() { setFlipped(!flipped_); }
    Q_INVOKABLE void toggleAutoplay();

    Q_INVOKABLE void fetchOnline(int platform, const QString &user, int maxGames);
    Q_INVOKABLE QString loadOnlineGame(int row);

    Q_INVOKABLE void copyToClipboard(const QString &text);
    Q_INVOKABLE QString toLocalFile(const QUrl &url) const { return url.toLocalFile(); }

signals:
    void gameChanged();
    void currentChanged();
    void flippedChanged();
    void autoplayChanged();
    void reviewChanged();
    void progressChanged();
    void onlineChanged();
    void settingsChanged();
    void toast(const QString &message, int kind);

private:
    QString loadGame(const zq::GameData &game);
    QString fenAt(int ply) const;
    QString displayFen() const;
    const QVector<zq::LearningPlyData> &activeLearningLine() const;
    void setPly(int ply);
    void rebuildPosition();
    void rebuildReviewDerived();
    void finishReview(bool ok, zq::ReviewData data, const QString &error);
    void stopWorker();
    QVariantMap makeCurrent() const;

    std::unique_ptr<zq::IBackend> backend_;
    QSettings settings_;
    QVariantList samples_;

    zq::GameData game_;
    QVariantMap gameInfo_;
    int ply_ = 0;
    bool flipped_ = false;
    QTimer autoplay_;

    QStringList squares_;
    bool whiteToMove_ = true;
    int lastFrom_ = -1, lastTo_ = -1, bestFrom_ = -1, bestTo_ = -1;
    QVariantMap current_, material_;

    // review
    int state_ = Idle;
    double progress_ = 0.0;
    QString reviewError_;
    bool hasReview_ = false;
    zq::ReviewData review_;
    QVariantMap summary_;
    QVariantList winSeries_, classSeries_, keyMoments_, learningMoments_;
    bool learningMode_ = false;
    int learningMistakePly_ = 0; // 1-based ply in the original game
    int learningLine_ = 0;        // 0 = engine's best line, 1 = played-move line
    int learningStep_ = 0;        // 0 = position before the mistake; 1..N = variation plies played
    PlyModel plyModel_;
    std::thread worker_;
    std::atomic<bool> cancel_{false};
    std::atomic<int> generation_{0};

    // online
    OnlineGamesModel onlineModel_;
    bool onlineBusy_ = false;
    bool onlineError_ = false;
    QString onlineStatus_;
    std::thread fetchThread_;

    // settings
    QString enginePath_;
    int depth_ = 14;
    int boardTheme_ = 0;
    bool showArrows_ = true;
    bool animateMoves_ = true;
    bool autoReview_ = false;
};
