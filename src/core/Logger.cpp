#include "Logger.h"
#include "Paths.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

namespace netraxis {
namespace {
Status logError() { return Status::failure({ErrorCode::Storage, "Cannot write the local event log.", false, "logging"}); }
const char *eventName(LogEvent e) {
    switch (e) {
    case LogEvent::Started: return "app.started";
    case LogEvent::Stopping: return "app.stopping";
    case LogEvent::OperationFailed: return "operation.failed";
    case LogEvent::SettingsChanged: return "settings.changed";
    case LogEvent::DataDeleted: return "data.deleted";
    }
    return "unknown";
}
}
Status Logger::open(qint64 maxBytes) {
    QMutexLocker guard(&mutex_);
    if (maxBytes < 4096 || maxBytes > 16 * 1024 * 1024)
        return Status::failure({ErrorCode::InvalidInput, "Invalid log size limit.", false, "logging"});
    const auto directory = applicationDataDirectory();
    if (!directory) return Status::failure(directory.error());
    const auto logs = directory.value() + "/logs";
    if (!QDir().mkpath(logs) || !QFile::setPermissions(logs, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner)) return logError();
    const auto path = logs + "/events.jsonl";
    if (QFileInfo(path).isSymLink() || QFileInfo(path + ".1").isSymLink()) return logError();
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append) || !file.setPermissions(QFile::ReadOwner | QFile::WriteOwner)) return logError();
    path_ = path;
    maxBytes_ = maxBytes;
    return success();
}
QVariantMap Logger::sanitized(const QVariantMap &fields) {
    QVariantMap out;
    for (auto it = fields.cbegin(); it != fields.cend() && out.size() < 32; ++it) {
        const auto &key = it.key();
        const auto &value = it.value();
        const auto type = value.metaType().id();
        const bool numeric = type == QMetaType::Int || type == QMetaType::LongLong || type == QMetaType::UInt || type == QMetaType::ULongLong;
        if ((key == "duration_ms" || key == "count" || key == "code") && numeric) out.insert(key, value);
        else if (key == "retryable" && type == QMetaType::Bool) out.insert(key, value);
        else out.insert("redacted_" + QString::number(out.size()), "[REDACTED]");
    }
    return out;
}
Status Logger::write(LogLevel level, LogEvent event, const QVariantMap &fields) {
    QMutexLocker guard(&mutex_);
    if (level < level_) return success();
    if (path_.isEmpty()) return logError();
    QJsonObject object = QJsonObject::fromVariantMap(sanitized(fields));
    object.insert("at", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    object.insert("level", int(level));
    object.insert("event", eventName(event));
    const auto line = QJsonDocument(object).toJson(QJsonDocument::Compact) + '\n';
    if (QFileInfo(path_).isSymLink() || QFileInfo(path_ + ".1").isSymLink()) return logError();
    if (QFileInfo(path_).size() + line.size() > maxBytes_) {
        if (QFile::exists(path_ + ".1") && !QFile::remove(path_ + ".1")) return logError();
        if (!QFile::rename(path_, path_ + ".1")) return logError();
    }
    QFile file(path_);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append) || !file.setPermissions(QFile::ReadOwner | QFile::WriteOwner)) return logError();
    if (file.write(line) != line.size() || !file.flush()) return logError();
    return success();
}
}
