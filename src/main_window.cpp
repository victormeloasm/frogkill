#include "main_window.h"
#include "process_model.h"
#include "system_sampler.h"
#include "util.h"

#include <QTableView>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTimer>
#include <QPushButton>
#include <QHeaderView>
#include <QMessageBox>
#include <QShortcut>
#include <QAction>
#include <QToolBar>
#include <QToolButton>
#include <QFrame>
#include <QMenu>
#include <QProcess>
#include <QApplication>
#include <QStyle>
#include <QStatusBar>
#include <QRegularExpression>
#include <QScreen>
#include <QDateTime>
#include <QCoreApplication>

#include <signal.h>
#include <errno.h>
#include <cstring>

#include <algorithm>
#include <unordered_map>
#include <vector>

namespace FrogKill {

static QString sigName(int sig) {
    if (sig == SIGTERM) return "TERM";
    if (sig == SIGKILL) return "KILL";
    return QString::number(sig);
}

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("FrogKill");
    resize(860, 520);

    setupActions();
    setupUi();
    setupShortcuts();
    applyViewTuning();

    // Refresh timer (only runs while the window is visible)
    m_timer = new QTimer(this);
    m_timer->setInterval(1000);
    connect(m_timer, &QTimer::timeout, this, &MainWindow::refreshNow);
}

MainWindow::~MainWindow() = default;

static QLabel* makeChip(QWidget* parent, const QString& text) {
    auto* l = new QLabel(text, parent);
    l->setObjectName("Chip");
    l->setAlignment(Qt::AlignCenter);
    l->setMinimumWidth(110);
    return l;
}

void MainWindow::setupUi() {
    auto* central = new QWidget(this);
    auto* root = new QVBoxLayout(central);

    // Header (title + compact live metrics + filter)
    m_header = new QFrame(this);
    m_header->setObjectName("Header");
    auto* headerLayout = new QHBoxLayout(m_header);
    headerLayout->setContentsMargins(12, 10, 12, 8);
    headerLayout->setSpacing(10);

    auto* title = new QLabel("FrogKill", this);
    title->setObjectName("Title");
    headerLayout->addWidget(title);

    headerLayout->addStretch(1);

    m_chipCpu = makeChip(this, "CPU —");
    m_chipMem = makeChip(this, "RAM —");
    m_chipProcs = makeChip(this, "Proc —");
    headerLayout->addWidget(m_chipCpu);
    headerLayout->addWidget(m_chipMem);
    headerLayout->addWidget(m_chipProcs);

    headerLayout->addStretch(1);

    m_filter = new QLineEdit(this);
    m_filter->setPlaceholderText("Filtrar (nome, PID, usuário)...");
    m_filter->setClearButtonEnabled(true);
    m_filter->setMaximumWidth(360);
    // Leading search icon (pure UI, negligible cost)
    m_filter->addAction(style()->standardIcon(QStyle::SP_FileDialogContentsView), QLineEdit::LeadingPosition);
    headerLayout->addWidget(m_filter);

    root->addWidget(m_header);

    // Toolbar (actions)
    m_toolbar = new QToolBar(this);
    m_toolbar->setMovable(false);
    m_toolbar->setFloatable(false);
    m_toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_toolbar->setIconSize(QSize(18, 18));
    m_toolbar->addAction(m_actRefresh);
    m_toolbar->addSeparator();
    m_toolbar->addAction(m_actKill);
    m_toolbar->addAction(m_actForce);
    m_toolbar->addSeparator();
    m_toolbar->addAction(m_actKillTree);
    m_toolbar->addAction(m_actForceTree);
    root->addWidget(m_toolbar);

    m_table = new QTableView(this);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setSortingEnabled(true);
    m_table->setAlternatingRowColors(true);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_table->verticalHeader()->setVisible(false);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setShowGrid(false);
    m_table->setWordWrap(false);
    // QTableView doesn't have setUniformRowHeights() (that's QTreeView).
    // Keep rows at a fixed height for consistent look and slightly cheaper layout.
    m_table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    m_table->verticalHeader()->setDefaultSectionSize(22);
    m_table->setTextElideMode(Qt::ElideRight);

    m_model = new ProcessModel(this);
    m_proxy = new QSortFilterProxyModel(this);
    m_proxy->setSourceModel(m_model);
    m_proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxy->setFilterKeyColumn(-1);

    m_table->setModel(m_proxy);
    root->addWidget(m_table, 1);

    // Status bar (bottom)
    statusBar()->showMessage("Pronto.");

    setCentralWidget(central);

    connect(m_filter, &QLineEdit::textChanged, this, [this](const QString& s){
        // substring match across all columns
        m_proxy->setFilterRegularExpression(QRegularExpression(QRegularExpression::escape(s),
                                                              QRegularExpression::CaseInsensitiveOption));
    });

    // Context menu
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_table, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos){
        QMenu menu(this);
        menu.addAction(m_actRefresh);
        menu.addSeparator();
        menu.addAction(m_actKill);
        menu.addAction(m_actForce);
        menu.addSeparator();
        menu.addAction(m_actKillTree);
        menu.addAction(m_actForceTree);
        menu.exec(m_table->viewport()->mapToGlobal(pos));
    });
}

void MainWindow::applyViewTuning() {
    if (!m_table) return;
    // Conservative fixed-ish defaults for a cleaner look without expensive ResizeToContents.
    m_table->setColumnWidth(0, 90);   // PID
    m_table->setColumnWidth(2, 90);   // CPU
    m_table->setColumnWidth(3, 110);  // RAM
    m_table->setColumnWidth(4, 140);  // User
    // Name (col 1) stays flexible.
}

void MainWindow::setupActions() {
    m_actRefresh = new QAction("Atualizar", this);
    m_actRefresh->setShortcut(QKeySequence::Refresh);
    m_actRefresh->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    m_actRefresh->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
    addAction(m_actRefresh);
    connect(m_actRefresh, &QAction::triggered, this, &MainWindow::refreshNow);

    m_actKill = new QAction("Finalizar", this);
    m_actKill->setShortcut(QKeySequence::Delete);
    m_actKill->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    m_actKill->setIcon(style()->standardIcon(QStyle::SP_DialogCloseButton));
    addAction(m_actKill);
    connect(m_actKill, &QAction::triggered, this, &MainWindow::killSelectedTerm);

    m_actForce = new QAction("Forçar", this);
    m_actForce->setShortcut(QKeySequence("Shift+Del"));
    m_actForce->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    m_actForce->setIcon(style()->standardIcon(QStyle::SP_MessageBoxCritical));
    addAction(m_actForce);
    connect(m_actForce, &QAction::triggered, this, &MainWindow::killSelectedKill);

    m_actKillTree = new QAction("Árvore", this);
    m_actKillTree->setShortcut(QKeySequence("Ctrl+Del"));
    m_actKillTree->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    m_actKillTree->setIcon(style()->standardIcon(QStyle::SP_DirIcon));
    addAction(m_actKillTree);
    connect(m_actKillTree, &QAction::triggered, this, &MainWindow::killSelectedTreeTerm);

    m_actForceTree = new QAction("Forçar árvore", this);
    m_actForceTree->setShortcut(QKeySequence("Ctrl+Shift+Del"));
    m_actForceTree->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    m_actForceTree->setIcon(style()->standardIcon(QStyle::SP_TrashIcon));
    addAction(m_actForceTree);
    connect(m_actForceTree, &QAction::triggered, this, &MainWindow::killSelectedTreeKill);
}

void MainWindow::setupShortcuts() {
    // Note: Global Ctrl+Shift+Esc should be configured in the desktop environment.
    // This shortcut only helps when the window already has focus.
    auto* localToggle = new QShortcut(QKeySequence("Ctrl+Shift+Escape"), this);
    connect(localToggle, &QShortcut::activated, this, &MainWindow::showAndRaise);
}

void MainWindow::showAndRaise() {
    show();
    raise();
    activateWindow();
}

void MainWindow::showEvent(QShowEvent* e) {
    QMainWindow::showEvent(e);
    refreshNow();
    if (m_timer) m_timer->start();
}

void MainWindow::hideEvent(QHideEvent* e) {
    QMainWindow::hideEvent(e);
    if (m_timer) m_timer->stop();
}

void MainWindow::refreshNow() {
    m_model->refresh();

    const auto snap = m_sys.sample();
    if (m_chipCpu) {
        m_chipCpu->setText(QString("CPU %1%")
                           .arg(snap.cpuPercent, 0, 'f', 1));
    }
    if (m_chipMem) {
        const double usedGiB = snap.memUsedMiB / 1024.0;
        const double totalGiB = snap.memTotalMiB / 1024.0;
        if (totalGiB > 0.0) {
            m_chipMem->setText(QString("RAM %1/%2 GiB")
                               .arg(usedGiB, 0, 'f', 1)
                               .arg(totalGiB, 0, 'f', 1));
        } else {
            m_chipMem->setText("RAM —");
        }
    }
    if (m_chipProcs) {
        m_chipProcs->setText(QString("Proc %1")
                             .arg(m_model->rowCount()));
    }

    statusBar()->showMessage(
        QString("Atualizado: %1")
            .arg(QDateTime::currentDateTime().toString("HH:mm:ss")),
        1500
    );
    if (m_table->model()->rowCount() > 0 && !m_table->currentIndex().isValid()) {
        m_table->selectRow(0);
    }
}

static bool askConfirm(QWidget* parent, const QString& title, const QString& msg) {
    const auto ret = QMessageBox::question(parent, title, msg,
                                          QMessageBox::Yes | QMessageBox::No,
                                          QMessageBox::No);
    return ret == QMessageBox::Yes;
}

void MainWindow::killSelectedTerm() {
    const auto idx = m_table->currentIndex();
    if (!idx.isValid()) return;

    const int srcRow = m_proxy->mapToSource(idx).row();
    const int pid = m_model->pidAtRow(srcRow);
    const QString name = m_model->nameAtRow(srcRow);

    if (!askConfirm(this, "Confirmar",
                    QString("Tem certeza que deseja finalizar \"%1\" (PID %2)?")
                        .arg(name).arg(pid))) {
        return;
    }

    tryKillPid(pid, SIGTERM, /*allowElevate=*/true);
    refreshNow();
}

void MainWindow::killSelectedKill() {
    const auto idx = m_table->currentIndex();
    if (!idx.isValid()) return;

    const int srcRow = m_proxy->mapToSource(idx).row();
    const int pid = m_model->pidAtRow(srcRow);
    const QString name = m_model->nameAtRow(srcRow);

    if (!askConfirm(this, "Confirmar",
                    QString("Tem certeza que deseja FORÇAR (SIGKILL) \"%1\" (PID %2)?")
                        .arg(name).arg(pid))) {
        return;
    }

    tryKillPid(pid, SIGKILL, /*allowElevate=*/true);
    refreshNow();
}

int MainWindow::estimateTreeSize(int rootPid) const {
    if (!m_model || rootPid <= 0) return 1;
    const int n = m_model->rowCount();
    std::unordered_map<int, std::vector<int>> children;
    children.reserve((size_t)n * 2u + 8u);
    for (int r = 0; r < n; ++r) {
        const int pid = m_model->pidAtRow(r);
        const int ppid = m_model->ppidAtRow(r);
        if (pid > 1 && ppid >= 0) {
            children[ppid].push_back(pid);
        }
    }
    std::vector<int> stack;
    stack.reserve(256);
    stack.push_back(rootPid);
    int count = 0;
    while (!stack.empty()) {
        const int pid = stack.back();
        stack.pop_back();
        ++count;
        auto it = children.find(pid);
        if (it == children.end()) continue;
        for (int c : it->second) stack.push_back(c);
    }
    return count;
}

static std::vector<int> buildTreePostorder(const ProcessModel* model, int rootPid) {
    std::vector<int> empty;
    if (!model || rootPid <= 0) return empty;
    const int n = model->rowCount();
    std::unordered_map<int, std::vector<int>> children;
    children.reserve((size_t)n * 2u + 8u);
    for (int r = 0; r < n; ++r) {
        const int pid = model->pidAtRow(r);
        const int ppid = model->ppidAtRow(r);
        if (pid > 1 && ppid >= 0) {
            children[ppid].push_back(pid);
        }
    }

    // Two-stack postorder: stack2 reversed yields children-first order.
    std::vector<int> stack1;
    std::vector<int> stack2;
    stack1.reserve(256);
    stack2.reserve(256);
    stack1.push_back(rootPid);
    while (!stack1.empty()) {
        const int pid = stack1.back();
        stack1.pop_back();
        stack2.push_back(pid);
        auto it = children.find(pid);
        if (it == children.end()) continue;
        for (int c : it->second) {
            stack1.push_back(c);
        }
    }
    std::reverse(stack2.begin(), stack2.end());
    return stack2;
}

void MainWindow::killSelectedTreeTerm() {
    const auto idx = m_table->currentIndex();
    if (!idx.isValid()) return;

    const int srcRow = m_proxy->mapToSource(idx).row();
    const int rootPid = m_model->pidAtRow(srcRow);
    const QString name = m_model->nameAtRow(srcRow);

    const int treeSize = estimateTreeSize(rootPid);
    const QString msg = (treeSize <= 1)
        ? QString("Tem certeza que deseja finalizar \"%1\" (PID %2)?").arg(name).arg(rootPid)
        : QString("Tem certeza que deseja finalizar a ÁRVORE de \"%1\" (PID %2)?\n\nIsso pode encerrar %3 processos.")
            .arg(name).arg(rootPid).arg(treeSize);

    if (!askConfirm(this, "Confirmar", msg)) return;

    if (rootPid <= 1) {
        QMessageBox::warning(this, "Bloqueado", "Por segurança, o FrogKill não finaliza PID <= 1.");
        return;
    }

    const auto order = buildTreePostorder(m_model, rootPid);
    const qint64 selfPid = QCoreApplication::applicationPid();

    for (int pid : order) {
        if (pid <= 1) continue;
        if (pid == (int)selfPid) continue;
        if (::kill(pid, SIGTERM) == 0) continue;
        const int e = errno;
        if (e == ESRCH) continue; // already gone
        if (e == EPERM) {
            // Ask once and use helper for the whole tree.
            const auto ret = QMessageBox::question(
                this,
                "Permissão necessária",
                QString("Sem permissão para finalizar toda a árvore do PID %1.\n\nExecutar como administrador (pedir senha)?").arg(rootPid),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No
            );
            if (ret == QMessageBox::Yes) {
                elevateKillPid(rootPid, SIGTERM, /*tree=*/true);
            }
            break;
        }
        QMessageBox::warning(this, "Erro",
                             QString("Falha ao finalizar PID %1 (SIGTERM): %2")
                                 .arg(pid)
                                 .arg(QString::fromLocal8Bit(std::strerror(e))));
        break;
    }
    refreshNow();
}

void MainWindow::killSelectedTreeKill() {
    const auto idx = m_table->currentIndex();
    if (!idx.isValid()) return;

    const int srcRow = m_proxy->mapToSource(idx).row();
    const int rootPid = m_model->pidAtRow(srcRow);
    const QString name = m_model->nameAtRow(srcRow);

    const int treeSize = estimateTreeSize(rootPid);
    const QString msg = (treeSize <= 1)
        ? QString("Tem certeza que deseja FORÇAR (SIGKILL) \"%1\" (PID %2)?").arg(name).arg(rootPid)
        : QString("Tem certeza que deseja FORÇAR (SIGKILL) a ÁRVORE de \"%1\" (PID %2)?\n\nIsso pode encerrar %3 processos.")
            .arg(name).arg(rootPid).arg(treeSize);

    if (!askConfirm(this, "Confirmar", msg)) return;

    if (rootPid <= 1) {
        QMessageBox::warning(this, "Bloqueado", "Por segurança, o FrogKill não finaliza PID <= 1.");
        return;
    }

    const auto order = buildTreePostorder(m_model, rootPid);
    const qint64 selfPid = QCoreApplication::applicationPid();

    for (int pid : order) {
        if (pid <= 1) continue;
        if (pid == (int)selfPid) continue;
        if (::kill(pid, SIGKILL) == 0) continue;
        const int e = errno;
        if (e == ESRCH) continue;
        if (e == EPERM) {
            const auto ret = QMessageBox::question(
                this,
                "Permissão necessária",
                QString("Sem permissão para forçar toda a árvore do PID %1.\n\nExecutar como administrador (pedir senha)?").arg(rootPid),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No
            );
            if (ret == QMessageBox::Yes) {
                elevateKillPid(rootPid, SIGKILL, /*tree=*/true);
            }
            break;
        }
        QMessageBox::warning(this, "Erro",
                             QString("Falha ao forçar PID %1 (SIGKILL): %2")
                                 .arg(pid)
                                 .arg(QString::fromLocal8Bit(std::strerror(e))));
        break;
    }
    refreshNow();
}

bool MainWindow::tryKillPid(int pid, int sig, bool allowElevate) {
    if (pid <= 1) {
        QMessageBox::warning(this, "Bloqueado", "Por segurança, o FrogKill não finaliza PID <= 1.");
        return false;
    }

    if (::kill(pid, sig) == 0) {
        return true;
    }

    const int e = errno;
    if (e == EPERM && allowElevate) {
        const auto ret = QMessageBox::question(
            this,
            "Permissão necessária",
            QString("Sem permissão para enviar SIG%1 ao PID %2.\n\nExecutar como administrador (pedir senha)?")
                .arg(sigName(sig)).arg(pid),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No
        );
        if (ret == QMessageBox::Yes) {
            return elevateKillPid(pid, sig, /*tree=*/false);
        }
        return false;
    }

    QMessageBox::warning(this, "Erro",
                         QString("Falha ao finalizar PID %1 (SIG%2): %3")
                             .arg(pid).arg(sigName(sig)).arg(QString::fromLocal8Bit(std::strerror(e))));
    return false;
}

bool MainWindow::elevateKillPid(int pid, int sig, bool tree) {
    // Call pkexec helper. Polkit will prompt the user for a password via the desktop auth agent.
    QString helper = QStringLiteral(FROGKILL_HELPER_PATH);

    QStringList args;
    args << "--disable-internal-agent"
         << helper
         << "--pid" << QString::number(pid)
         << "--sig" << sigName(sig);
    if (tree) {
        args << "--tree";
    }

    QProcess p;
    p.setProgram("pkexec");
    p.setArguments(args);
    p.setProcessChannelMode(QProcess::MergedChannels);

    p.start();
    if (!p.waitForFinished(-1)) {
        QMessageBox::warning(this, "Erro", "Falha ao executar pkexec.");
        return false;
    }

    const int code = p.exitCode();
    const QString out = QString::fromLocal8Bit(p.readAll());

    if (code == 0) return true;

    QMessageBox::warning(this, "Erro",
                         QString("Helper retornou código %1.\n\nSaída:\n%2")
                             .arg(code).arg(out.trimmed()));
    return false;
}

} // namespace FrogKill
