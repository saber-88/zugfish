#include <QCommandLineParser>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSurfaceFormat>
#include <QTimer>
#include <QImage>
#include <memory>

#include "AppController.h"
#include "backend/DemoBackend.h"
#ifdef ZUGFISH_WITH_CORE
#include "backend/ZugfishBackend.h"
#endif

static std::unique_ptr<zq::IBackend> makeBackend()
{
#ifdef ZUGFISH_WITH_CORE
    return std::make_unique<zq::ZugfishBackend>();
#else
    return std::make_unique<zq::DemoBackend>();
#endif
}

int main(int argc, char *argv[])
{
    QSurfaceFormat format;
    format.setSamples(4);                       // smooth edges for Shape items (icons, arrows)
    QSurfaceFormat::setDefaultFormat(format);

    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("zugfish"));
    app.setOrganizationName(QStringLiteral("zugfish"));
    app.setApplicationDisplayName(QStringLiteral("zugfish"));
    QQuickStyle::setStyle(QStringLiteral("Basic"));   // we draw everything ourselves

    for (const char *face : {"IBMPlexSans-Regular", "IBMPlexSans-Medium", "IBMPlexSans-SemiBold",
                             "IBMPlexSans-Bold", "IBMPlexMono-Medium"})
        QFontDatabase::addApplicationFont(QStringLiteral(":/assets/fonts/%1.ttf").arg(QLatin1String(face)));
    QFont font(QStringLiteral("IBM Plex Sans"));
    font.setPixelSize(13);
    app.setFont(font);

    // Developer options: handy for UI work and for taking screenshots in CI.
    QCommandLineParser cli;
    cli.setApplicationDescription(QStringLiteral("zugfish: Stockfish-powered game review"));
    cli.addHelpOption();
    cli.addPositionalArgument(QStringLiteral("pgn"), QStringLiteral("PGN file to open"), QStringLiteral("[pgn]"));
    cli.addOption({"sample", "Load a bundled sample game (opera, immortal, legall)", "id"});
    cli.addOption({"review", "Start the review right away"});
    cli.addOption({"ply", "Jump to this ply once loaded", "n"});
    cli.addOption({"tab", "Right-hand tab: 0 Review, 1 Moves, 2 Details", "n"});
    cli.addOption({"dialog", "Open a dialog: import | settings", "name"});
    cli.addOption({"flip", "Start with the board flipped"});
    cli.addOption({"nowait", "With --screenshot: do not wait for the review to finish"});
    cli.addOption({"size", "Window size, e.g. 1360x860", "WxH"});
    cli.addOption({"step", "After --ply, step this many moves forward (exercises the move animation)", "n"});
    cli.addOption({"delay", "Milliseconds to wait before the screenshot (default 1400)", "ms"});
    cli.addOption({"screenshot", "Save a screenshot to this file and exit", "file"});
    cli.process(app);

    AppController controller(makeBackend());

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("App"), &controller);
    engine.load(QUrl(QStringLiteral("qrc:/qt/qml/Zugfish/Main.qml")));
    if (engine.rootObjects().isEmpty())
        return 1;
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());

    if (cli.isSet("size")) {
        const QStringList wh = cli.value("size").split('x');
        if (wh.size() == 2)
            window->resize(wh[0].toInt(), wh[1].toInt());
    }
    if (!cli.positionalArguments().isEmpty())
        controller.loadPgnFile(QUrl::fromLocalFile(cli.positionalArguments().first()));
    if (cli.isSet("sample"))
        controller.loadSample(cli.value("sample"));
    if (cli.isSet("flip"))
        controller.setFlipped(true);
    if (cli.isSet("review"))
        controller.startReview();
    if (cli.isSet("tab"))
        window->setProperty("tabIndex", cli.value("tab").toInt());

    const bool waitForReview = cli.isSet("review") && !cli.isSet("nowait");
    const int plyArg = cli.isSet("ply") ? cli.value("ply").toInt() : -1;
    bool plyApplied = false;

    // Apply --ply / --dialog / --screenshot once the review (if requested) has finished.
    auto *poll = new QTimer(&app);
    poll->setInterval(100);
    QObject::connect(poll, &QTimer::timeout, &app, [&, poll] {
        if (waitForReview && controller.reviewState() != AppController::Done) {
            if (controller.reviewState() == AppController::Failed)
                QCoreApplication::exit(2);
            return;
        }
        if (!plyApplied) {
            plyApplied = true;
            if (plyArg >= 0)
                controller.goTo(plyArg);
            if (cli.isSet("dialog"))
                QMetaObject::invokeMethod(window, "openDialog", Q_ARG(QVariant, cli.value("dialog")));
            if (cli.isSet("step"))
                for (int i = 0; i < cli.value("step").toInt(); ++i)
                    controller.next();
            if (cli.isSet("screenshot"))
                QTimer::singleShot(cli.isSet("delay") ? cli.value("delay").toInt() : 1400, &app, [&] {   // let animations settle
                    window->grabWindow().save(cli.value("screenshot"));
                    QCoreApplication::quit();
                });
            poll->stop();
        }
    });
    poll->start();

    return app.exec();
}
