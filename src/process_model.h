#pragma once
#include <QAbstractTableModel>
#include <vector>
#include "procfs.h"

namespace FrogKill {

class ProcessModel : public QAbstractTableModel {
    Q_OBJECT
public:
    explicit ProcessModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;

    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    void refresh();

    int pidAtRow(int row) const;
    int ppidAtRow(int row) const;
    QString nameAtRow(int row) const;

private:
    std::vector<ProcInfo> m_rows;
    ProcSampler m_sampler;
};

} // namespace FrogKill
