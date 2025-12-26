#include "app_controller.h"
#include <QLocalSocket>
#include <QFileInfo>
#include <QByteArray>
#include <QDebug>
#include <QApplication>
#include <QStyle>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include "main_window.h"

namespace FrogKill {

static constexpr const char* kServerName = "frogkill_ipc_v1";

AppController::AppController(QObject* parent) : QObject(parent) {}

AppController::~AppController() = default;

bool AppController::startServer() {
    if (!m_singleInstance) {
        ensureWindow();
        return true;
    }

    if (m_server.isListening()) {
        return true;
    }

    // Remove stale socket if any.
    QLocalServer::removeServer(kServerName);

    if (!m_server.listen(kServerName)) {
        qWarning() << "Failed to listen on" << kServerName << ":" << m_server.errorString();
        return false;
    }

    connect(&m_server, &QLocalServer::newConnection, this, &AppController::onNewConnection);
    return true;
}

bool AppController::trySendCommand(const QByteArray& cmd) {
    if (!m_singleInstance) return false;

    QLocalSocket sock;
    sock.connectToServer(kServerName, QIODevice::WriteOnly);
    if (!sock.waitForConnected(120)) {
        return false;
    }
    sock.write(cmd);
    sock.write("\n");
    sock.flush();
    sock.waitForBytesWritten(200);
    sock.disconnectFromServer();
    return true;
}

void AppController::ensureWindow() {
    if (!m_window) {
        m_window = new MainWindow();
        connect(m_window, &QObject::destroyed, this, [this] { m_window = nullptr; });
    }
}

void AppController::ensureTray() {
    if (m_tray) return;
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        qWarning() << "System tray is not available; running without tray.";
        return;
    }

    QIcon icon = QApplication::style()->standardIcon(QStyle::SP_ComputerIcon);
    m_tray = new QSystemTrayIcon(icon, this);
    m_tray->setToolTip("FrogKill");

    m_trayMenu = new QMenu();
    m_trayShow = m_trayMenu->addAction("Abrir");
    m_trayToggle = m_trayMenu->addAction("Mostrar/ocultar");
    m_trayMenu->addSeparator();
    m_trayQuit = m_trayMenu->addAction("Sair");
    m_tray->setContextMenu(m_trayMenu);

    QObject::connect(m_trayShow, &QAction::triggered, this, &AppController::showWindow);
    QObject::connect(m_trayToggle, &QAction::triggered, this, &AppController::toggleWindow);
    QObject::connect(m_trayQuit, &QAction::triggered, qApp, &QCoreApplication::quit);

    // Single click toggles, double click opens.
    QObject::connect(m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason){
        if (reason == QSystemTrayIcon::Trigger) {
            toggleWindow();
        } else if (reason == QSystemTrayIcon::DoubleClick) {
            showWindow();
        }
    });

    m_tray->show();
}

void AppController::showWindow() {
    ensureWindow();
    m_window->showAndRaise();
}

void AppController::toggleWindow() {
    ensureWindow();
    if (m_window->isVisible()) {
        // If it's visible but not active, raise; otherwise hide.
        if (!m_window->isActiveWindow()) {
            m_window->showAndRaise();
        } else {
            m_window->hide();
        }
    } else {
        m_window->showAndRaise();
    }
}

void AppController::onNewConnection() {
    while (auto* c = m_server.nextPendingConnection()) {
        QObject::connect(c, &QLocalSocket::readyRead, c, [this, c] {
            const QByteArray data = c->readAll().trimmed();
            if (data == "toggle") {
                toggleWindow();
            } else if (data == "show") {
                showWindow();
            } else if (data == "hide") {
                ensureWindow();
                m_window->hide();
            }
            c->disconnectFromServer();
        });
    }
}

} // namespace FrogKill
