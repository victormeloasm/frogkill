#include <cerrno>
#include <csignal>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <unistd.h>

static void usage() {
    std::cerr << "frogkill-helper --pid <PID> --sig TERM|KILL [--tree]\n";
}

static bool parseInt(const std::string& s, long long& out) {
    char* end = nullptr;
    errno = 0;
    const long long v = std::strtoll(s.c_str(), &end, 10);
    if (errno != 0 || !end || *end != '\0') return false;
    out = v;
    return true;
}

int main(int argc, char** argv) {
    int pid = -1;
    int sig = 0;
    bool tree = false;

    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--pid" && i + 1 < argc) {
            long long v = 0;
            if (!parseInt(argv[++i], v)) {
                std::cerr << "Invalid PID\n";
                return 2;
            }
            pid = (int)v;
        } else if (a == "--sig" && i + 1 < argc) {
            std::string s = argv[++i];
            if (s == "TERM") sig = SIGTERM;
            else if (s == "KILL") sig = SIGKILL;
            else {
                std::cerr << "Invalid signal (allowed: TERM|KILL)\n";
                return 2;
            }
        } else if (a == "--help" || a == "-h") {
            usage();
            return 0;
        } else if (a == "--tree") {
            tree = true;
        } else {
            std::cerr << "Unknown argument: " << a << "\n";
            usage();
            return 2;
        }
    }

    if (pid <= 0 || sig == 0) {
        usage();
        return 2;
    }

    if (pid <= 1) {
        std::cerr << "Refusing to signal PID <= 1\n";
        return 3;
    }

    // Validate existence
    if (kill(pid, 0) != 0 && errno == ESRCH) {
        std::cerr << "PID does not exist\n";
        return 3;
    }

    auto doKill = [&](int targetPid) -> bool {
        if (targetPid <= 1) return true;
        if (kill(targetPid, sig) == 0) return true;
        if (errno == ESRCH) return true;
        std::cerr << "kill(" << targetPid << "," << sig << ") failed: " << std::strerror(errno) << "\n";
        return false;
    };

    if (!tree) {
        if (!doKill(pid)) return 4;
        return 0;
    }

    // --- Tree mode: build PPID -> children map from /proc and kill children first. ---
    std::unordered_map<int, std::vector<int>> children;
    children.reserve(8192);

    auto readPpid = [](int p, int& outPpid) -> bool {
        std::string path = "/proc/" + std::to_string(p) + "/stat";
        std::ifstream f(path);
        if (!f) return false;
        std::string line;
        std::getline(f, line);
        if (line.empty()) return false;
        const auto rparen = line.rfind(')');
        if (rparen == std::string::npos || rparen + 2 >= line.size()) return false;
        std::istringstream iss(line.substr(rparen + 2));
        char state = 0;
        int ppid = -1;
        iss >> state >> ppid;
        if (!iss || ppid < 0) return false;
        outPpid = ppid;
        return true;
    };

    for (const auto& entry : std::filesystem::directory_iterator("/proc")) {
        if (!entry.is_directory()) continue;
        const auto name = entry.path().filename().string();
        if (name.empty() || name[0] < '0' || name[0] > '9') continue;
        long long v = 0;
        if (!parseInt(name, v)) continue;
        const int childPid = (int)v;
        int ppid = -1;
        if (!readPpid(childPid, ppid)) continue;
        if (childPid > 1 && ppid >= 0) {
            children[ppid].push_back(childPid);
        }
    }

    std::vector<int> stack1;
    std::vector<int> stack2;
    stack1.reserve(256);
    stack2.reserve(256);
    stack1.push_back(pid);
    while (!stack1.empty()) {
        const int cur = stack1.back();
        stack1.pop_back();
        stack2.push_back(cur);
        auto it = children.find(cur);
        if (it == children.end()) continue;
        for (int c : it->second) stack1.push_back(c);
    }
    std::reverse(stack2.begin(), stack2.end());

    int ok = 0;
    for (int target : stack2) {
        if (doKill(target)) ++ok;
        else return 4;
    }
    (void)ok;

    return 0;
}
