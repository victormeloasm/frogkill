#include "process_model.h"
#include <QLocale>

namespace FrogKill {

ProcessModel::ProcessModel(QObject* parent) : QAbstractTableModel(parent) {}

int ProcessModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return static_cast<int>(m_rows.size());
}

int ProcessModel::columnCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return 5; // PID, Name, CPU, RSS, User
}

QVariant ProcessModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) return {};
    switch (section) {
        case 0: return "PID";
        case 1: return "Processo";
        case 2: return "CPU %";
        case 3: return "RAM (MiB)";
        case 4: return "Usuário";
        default: return {};
    }
}

QVariant ProcessModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid()) return {};
    const int r = index.row();
    const int c = index.column();
    if (r < 0 || r >= (int)m_rows.size()) return {};

    const auto& p = m_rows[(size_t)r];

    if (role == Qt::DisplayRole) {
        switch (c) {
            case 0: return p.pid;
            case 1: return p.name;
            case 2: return QString::number(p.cpuPercent, 'f', 1);
            case 3: return QString::number(p.rssMiB, 'f', 1);
            case 4: return p.user;
        }
    }

    if (role == Qt::TextAlignmentRole) {
        if (c == 0 || c == 2 || c == 3) return Qt::AlignRight;
    }

    return {};
}

void ProcessModel::refresh() {
    beginResetModel();
    m_rows = m_sampler.sample();
    endResetModel();
}

int ProcessModel::pidAtRow(int row) const {
    if (row < 0 || row >= (int)m_rows.size()) return -1;
    return m_rows[(size_t)row].pid;
}

int ProcessModel::ppidAtRow(int row) const {
    if (row < 0 || row >= (int)m_rows.size()) return -1;
    return m_rows[(size_t)row].ppid;
}

QString ProcessModel::nameAtRow(int row) const {
    if (row < 0 || row >= (int)m_rows.size()) return {};
    return m_rows[(size_t)row].name;
}

} // namespace FrogKill
