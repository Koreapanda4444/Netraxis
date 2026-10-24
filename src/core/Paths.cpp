#include "Paths.h"
#include <QDir>
#include <QFile>
#include <QStandardPaths>

namespace netraxis {
Result<QString> applicationDataDirectory() {
    const auto path = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (path.isEmpty() || !QDir().mkpath(path))
        return Result<QString>::failure({ErrorCode::PermissionDenied, "Cannot create the application data directory.", false, "startup"});
    const QFile::Permissions owner = QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner;
    if (!QFile::setPermissions(path, owner))
        return Result<QString>::failure({ErrorCode::PermissionDenied, "Cannot restrict access to the application data directory.", false, "startup"});
    return Result<QString>::success(path);
}
}
