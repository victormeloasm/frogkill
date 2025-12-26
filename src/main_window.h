#pragma once
#include <QMainWindow>
#include <QSortFilterProxyModel>

#include "system_sampler.h"

// Forward declarations MUST be in the global namespace. If you write
// `class QLineEdit*` inside namespace FrogKill, you accidentally declare
// FrogKill::QLineEdit instead of ::QLineEdit.
class QLineEdit;
class QTableView;
class QLabel;
class QTimer;
class QAction;
class QToolBar;
class QFrame;

namespace FrogKill {

class ProcessModel;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    void showAndRaise();

private slots:
    void refreshNow();
    void killSelectedTerm();
    void killSelectedKill();
    void killSelectedTreeTerm();
    void killSelectedTreeKill();

private:
    void setupActions();
    bool tryKillPid(int pid, int sig, bool allowElevate);
    bool elevateKillPid(int pid, int sig, bool tree);

    // For tree operations we compute the list in the GUI for confirmation only.
    // The actual termination may be done either directly (user has permission)
    // or via the polkit helper (pkexec) using --tree.
    int estimateTreeSize(int rootPid) const;
    void setupUi();
    void setupShortcuts(); // keeps local Ctrl+Shift+Esc while window is focused
    void applyViewTuning();

protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;

    ProcessModel* m_model{nullptr};
    QSortFilterProxyModel* m_proxy{nullptr};

    QLineEdit* m_filter{nullptr};
    QTableView* m_table{nullptr};
    QLabel* m_chipCpu{nullptr};
    QLabel* m_chipMem{nullptr};
    QLabel* m_chipProcs{nullptr};
    QTimer* m_timer{nullptr};

    QToolBar* m_toolbar{nullptr};
    QFrame* m_header{nullptr};

    SystemSampler m_sys{};

    QAction* m_actRefresh{nullptr};

    QAction* m_actKill{nullptr};
    QAction* m_actForce{nullptr};

    QAction* m_actKillTree{nullptr};
    QAction* m_actForceTree{nullptr};
};

} // namespace FrogKill
