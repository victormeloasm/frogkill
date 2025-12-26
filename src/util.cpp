#include "util.h"
#include <pwd.h>
#include <unistd.h>

namespace FrogKill::Util {

QString trimmed(QString s) {
    s = s.trimmed();
    // collapse excessive spaces
    while (s.contains("  ")) s.replace("  ", " ");
    return s;
}

QString usernameFromUid(uid_t uid) {
    struct passwd pwd;
    struct passwd* result = nullptr;
    char buf[4096];
    if (getpwuid_r(uid, &pwd, buf, sizeof(buf), &result) == 0 && result && result->pw_name) {
        return QString::fromLocal8Bit(result->pw_name);
    }
    return QString::number((qulonglong)uid);
}

} // namespace FrogKill::Util
