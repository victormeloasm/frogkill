#pragma once
#include <QString>
#include <vector>
#include <unordered_map>

namespace FrogKill {

struct ProcInfo {
    int pid{};
    int ppid{};
    QString name;
    QString user;
    double cpuPercent{0.0};
    double rssMiB{0.0};
};

class ProcSampler {
public:
    std::vector<ProcInfo> sample();

private:
    long long readTotalJiffies() const;

    struct Prev {
        long long procJiffies{0};
    };

    long long m_prevTotalJiffies{0};
    std::unordered_map<int, Prev> m_prevByPid;
};

} // namespace FrogKill
