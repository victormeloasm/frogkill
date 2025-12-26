#include <QApplication>
#include <QCommandLineParser>
#include <QStyleFactory>
#include "app_controller.h"

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("FrogKill");
    QApplication::setOrganizationName("FrogTools");

    // UI polish with near-zero runtime cost.
    // Fusion tends to look consistent across desktops; we keep the system palette.
    app.setStyle(QStyleFactory::create("Fusion"));
    app.setStyleSheet(
        "QFrame#Header { padding: 0px; }"
        "QLabel#Title { font-size: 18px; font-weight: 700; padding-left: 2px; }"
        "QLabel#Chip { padding: 5px 10px; border-radius: 10px; "
        "  border: 1px solid palette(mid); background: palette(base); }"
        "QToolBar { border: none; padding: 4px 6px; spacing: 6px; }"
        "QToolButton { padding: 6px 10px; border-radius: 10px; }"
        "QToolButton:hover { background: palette(alternate-base); }"
        "QLineEdit { padding: 8px 10px; border-radius: 12px; "
        "  border: 1px solid palette(mid); background: palette(base); }"
        "QTableView { border: 1px solid palette(mid); border-radius: 12px; }"
        "QHeaderView::section { padding: 6px 8px; font-weight: 600; "
        "  border: none; background: palette(window); }"
        "QTableView::item { padding: 4px 6px; }"
        "QStatusBar { border: none; padding: 4px 10px; }"
    );

    QCommandLineParser parser;
    parser.setApplicationDescription("FrogKill - Windows-like task manager for Linux");
    parser.addHelpOption();

    QCommandLineOption optDaemon(QStringList{} << "d" << "daemon", "Run in background and listen for toggle commands.");
    QCommandLineOption optToggle(QStringList{} << "t" << "toggle", "Toggle/raise the FrogKill window (IPC to running daemon).");
    QCommandLineOption optNoSingle(QStringList{} << "no-single-instance", "Disable single-instance behavior (debug only).");

    parser.addOption(optDaemon);
    parser.addOption(optToggle);
    parser.addOption(optNoSingle);

    parser.process(app);

    FrogKill::AppController controller;
    controller.setSingleInstanceEnabled(!parser.isSet(optNoSingle));

    if (parser.isSet(optToggle)) {
        // Try to toggle an existing instance; if none is running, fall back to starting normally.
        if (controller.trySendCommand("toggle")) {
            return 0;
        }
        // No running instance. Start GUI directly.
        controller.startServer();
        controller.showWindow();
        return app.exec();
    }

    if (parser.isSet(optDaemon)) {
        controller.startServer();
        // Daemon mode: keep a tray icon so the user can toggle quickly.
        controller.ensureTray();
        // No window yet; waits for toggle.
        return app.exec();
    }

    // Default: show GUI (and still be single-instance capable)
    controller.startServer();
    controller.showWindow();
    return app.exec();
}
