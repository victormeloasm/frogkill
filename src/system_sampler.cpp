#include "system_sampler.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace FrogKill {

static bool readCpuLine(unsigned long long& total, unsigned long long& idle) {
    // Read first line of /proc/stat: cpu  user nice system idle iowait irq softirq steal ...
    FILE* f = std::fopen("/proc/stat", "r");
    if (!f) return false;
    char tag[8] = {0};
    unsigned long long user=0, nice=0, sys=0, idle0=0, iowait=0, irq=0, soft=0, steal=0;
    int n = std::fscanf(f, "%7s %llu %llu %llu %llu %llu %llu %llu %llu",
                        tag, &user, &nice, &sys, &idle0, &iowait, &irq, &soft, &steal);
    std::fclose(f);
    if (n < 5) return false;
    const unsigned long long idleAll = idle0 + iowait;
    const unsigned long long nonIdle = user + nice + sys + irq + soft + steal;
    total = idleAll + nonIdle;
    idle = idleAll;
    return true;
}

static void readMemInfo(double& memTotalMiB, double& memUsedMiB, double& swapTotalMiB, double& swapUsedMiB) {
    // Parse just what we need from /proc/meminfo. Values are in kB.
    FILE* f = std::fopen("/proc/meminfo", "r");
    if (!f) return;

    unsigned long long memTotalKB = 0;
    unsigned long long memAvailKB = 0;
    unsigned long long swapTotalKB = 0;
    unsigned long long swapFreeKB = 0;

    char key[64];
    unsigned long long val = 0;
    char unit[16];

    while (std::fscanf(f, "%63[^:]: %llu %15s\n", key, &val, unit) == 3) {
        if (key[0] == 'M' && key[1] == 'e') {
            if (std::strcmp(key, "MemTotal") == 0) memTotalKB = val;
            else if (std::strcmp(key, "MemAvailable") == 0) memAvailKB = val;
        } else if (key[0] == 'S' && key[1] == 'w') {
            if (std::strcmp(key, "SwapTotal") == 0) swapTotalKB = val;
            else if (std::strcmp(key, "SwapFree") == 0) swapFreeKB = val;
        }
        if (memTotalKB && memAvailKB && swapTotalKB && swapFreeKB) {
            // We already have everything; break early.
            break;
        }
    }
    std::fclose(f);

    if (memTotalKB) {
        const unsigned long long usedKB = (memAvailKB <= memTotalKB) ? (memTotalKB - memAvailKB) : 0;
        memTotalMiB = (double)memTotalKB / 1024.0;
        memUsedMiB = (double)usedKB / 1024.0;
    }
    if (swapTotalKB) {
        const unsigned long long usedKB = (swapFreeKB <= swapTotalKB) ? (swapTotalKB - swapFreeKB) : 0;
        swapTotalMiB = (double)swapTotalKB / 1024.0;
        swapUsedMiB = (double)usedKB / 1024.0;
    }
}

SystemSnapshot SystemSampler::sample() {
    SystemSnapshot s;

    unsigned long long total=0, idle=0;
    if (readCpuLine(total, idle)) {
        if (m_prevTotal != 0 && total > m_prevTotal) {
            const unsigned long long dt = total - m_prevTotal;
            const unsigned long long di = (idle >= m_prevIdle) ? (idle - m_prevIdle) : 0;
            const unsigned long long busy = (dt >= di) ? (dt - di) : 0;
            s.cpuPercent = (dt ? (100.0 * (double)busy / (double)dt) : 0.0);
        }
        m_prevTotal = total;
        m_prevIdle = idle;
    }

    readMemInfo(s.memTotalMiB, s.memUsedMiB, s.swapTotalMiB, s.swapUsedMiB);
    return s;
}

} // namespace FrogKill
