#pragma once

namespace FrogKill {

struct SystemSnapshot {
    double cpuPercent{0.0};
    double memUsedMiB{0.0};
    double memTotalMiB{0.0};
    double swapUsedMiB{0.0};
    double swapTotalMiB{0.0};
};

// Tiny /proc sampler for a compact UI header (CPU + RAM + Swap).
// Designed to keep allocations and parsing minimal.
class SystemSampler {
public:
    SystemSnapshot sample();

private:
    // CPU deltas
    unsigned long long m_prevTotal{0};
    unsigned long long m_prevIdle{0};
};

} // namespace FrogKill
