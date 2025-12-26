#pragma once
#include <QObject>
#include <QLocalServer>

class QSystemTrayIcon;
class QMenu;
class QAction;

namespace FrogKill {

class MainWindow;

class AppController : public QObject {
    Q_OBJECT
public:
    explicit AppController(QObject* parent = nullptr);
    ~AppController() override;

    void setSingleInstanceEnabled(bool enabled) { m_singleInstance = enabled; }

    // Starts the IPC server (single instance). Safe to call multiple times.
    bool startServer();

    // Attempts to send a command to an already-running instance.
    bool trySendCommand(const QByteArray& cmd);

    // Creates + shows the main window (or raises it).
    void showWindow();
    void toggleWindow();

    // Create tray icon + menu (no-op if system tray is unavailable).
    void ensureTray();

private slots:
    void onNewConnection();

private:
    void ensureWindow();

    bool m_singleInstance{true};
    QLocalServer m_server;
    MainWindow* m_window{nullptr};

    QSystemTrayIcon* m_tray{nullptr};
    QMenu* m_trayMenu{nullptr};
    QAction* m_trayShow{nullptr};
    QAction* m_trayToggle{nullptr};
    QAction* m_trayQuit{nullptr};
};

} // namespace FrogKill
