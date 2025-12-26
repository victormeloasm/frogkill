#include "procfs.h"
#include "util.h"

#include <QFile>
#include <QByteArray>
#include <QStringList>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <cctype>

#include <unistd.h>

namespace fs = std::filesystem;

namespace FrogKill {

static bool isDigits(const std::string& s) {
    return !s.empty() && std::all_of(s.begin(), s.end(), [](unsigned char ch){ return std::isdigit(ch); });
}

static bool readFileToString(const std::string& path, std::string& out) {
    std::ifstream f(path, std::ios::in | std::ios::binary);
    if (!f) return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}

long long ProcSampler::readTotalJiffies() const {
    std::ifstream f("/proc/stat");
    if (!f) return 0;
    std::string cpu;
    f >> cpu;
    if (cpu != "cpu") return 0;
    long long v = 0, sum = 0;
    while (f >> v) {
        sum += v;
    }
    return sum;
}

std::vector<ProcInfo> ProcSampler::sample() {
    const int cores = (int)sysconf(_SC_NPROCESSORS_ONLN);
    const long long totalJ = readTotalJiffies();
    const long long prevTotal = m_prevTotalJiffies;
    const long long deltaTotal = (prevTotal > 0 && totalJ > prevTotal) ? (totalJ - prevTotal) : 0;
    m_prevTotalJiffies = totalJ;

    std::vector<ProcInfo> out;
    out.reserve(1024);

    for (const auto& entry : fs::directory_iterator("/proc")) {
        if (!entry.is_directory()) continue;
        const std::string pidStr = entry.path().filename().string();
        if (!isDigits(pidStr)) continue;

        const int pid = std::stoi(pidStr);

        // Parse /proc/[pid]/stat for comm, ppid, utime, stime
        std::string statLine;
        if (!readFileToString("/proc/" + pidStr + "/stat", statLine)) continue;

        const auto lpar = statLine.find('(');
        const auto rpar = statLine.rfind(')');
        if (lpar == std::string::npos || rpar == std::string::npos || rpar <= lpar) continue;

        const std::string comm = statLine.substr(lpar + 1, rpar - lpar - 1);
        const std::string rest = statLine.substr(rpar + 2); // after ") "
        std::istringstream rs(rest);

        std::vector<std::string> tok;
        tok.reserve(64);
        std::string t;
        while (rs >> t) tok.push_back(t);
        if (tok.size() < 13) continue;

        const int ppid = std::stoi(tok[1]);     // field4
        const long long utime = std::stoll(tok[11]); // field14
        const long long stime = std::stoll(tok[12]); // field15
        const long long procJ = utime + stime;

        // Read /proc/[pid]/status for VmRSS and Uid
        std::string status;
        long long rssKb = 0;
        uid_t uid = 0;
        if (readFileToString("/proc/" + pidStr + "/status", status)) {
            std::istringstream ss(status);
            std::string line;
            while (std::getline(ss, line)) {
                if (line.rfind("VmRSS:", 0) == 0) {
                    // VmRSS:   12345 kB
                    std::istringstream ls(line);
                    std::string key, unit;
                    ls >> key >> rssKb >> unit;
                } else if (line.rfind("Uid:", 0) == 0) {
                    std::istringstream ls(line);
                    std::string key;
                    long long ruid = 0;
                    ls >> key >> ruid;
                    uid = (uid_t)ruid;
                }
            }
        }

        // Read cmdline (optional)
        std::string cmdline;
        QString display = QString::fromStdString(comm);
        if (readFileToString("/proc/" + pidStr + "/cmdline", cmdline) && !cmdline.empty()) {
            // cmdline is NUL-separated
            for (auto& ch : cmdline) if (ch == '\0') ch = ' ';
            const auto trimmed = Util::trimmed(QString::fromStdString(cmdline));
            if (!trimmed.isEmpty()) display = trimmed;
        }

        ProcInfo info;
        info.pid = pid;
        info.ppid = ppid;
        info.name = display;
        info.user = Util::usernameFromUid(uid);
        info.rssMiB = rssKb / 1024.0;

        // CPU %
        double cpu = 0.0;
        auto it = m_prevByPid.find(pid);
        if (deltaTotal > 0 && it != m_prevByPid.end()) {
            const long long deltaProc = procJ - it->second.procJiffies;
            if (deltaProc > 0) {
                cpu = 100.0 * (double)deltaProc / (double)deltaTotal * (double)cores;
                if (cpu < 0) cpu = 0;
                const double maxCpu = 100.0 * (double)cores;
                if (cpu > maxCpu) cpu = maxCpu;
            }
        }
        info.cpuPercent = cpu;

        m_prevByPid[pid] = Prev{procJ};
        out.push_back(std::move(info));
    }

    // Keep list deterministic-ish: sort by CPU descending by default.
    std::sort(out.begin(), out.end(), [](const ProcInfo& a, const ProcInfo& b){
        if (a.cpuPercent != b.cpuPercent) return a.cpuPercent > b.cpuPercent;
        return a.pid < b.pid;
    });

    // Prune prev map for dead pids occasionally (cheap O(n) for our out size).
    {
        std::unordered_map<int, Prev> next;
        next.reserve(out.size() * 2 + 64);
        for (const auto& p : out) {
            auto it = m_prevByPid.find(p.pid);
            if (it != m_prevByPid.end()) next.emplace(p.pid, it->second);
        }
        m_prevByPid.swap(next);
    }

    return out;
}

} // namespace FrogKill
