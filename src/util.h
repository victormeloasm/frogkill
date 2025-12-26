#pragma once
#include <QString>
#include <sys/types.h>

namespace FrogKill::Util {

QString trimmed(QString s);
QString usernameFromUid(uid_t uid);

} // namespace FrogKill::Util
