#pragma once
#include "Result.h"
#include <QMutex>
#include <QVariantMap>

namespace netraxis {
enum class LogLevel { Debug, Info, Warning, Error };
enum class LogEvent { Started, Stopping, OperationFailed, SettingsChanged, DataDeleted };
class Logger final {
public:
    Status open(qint64 maxBytes = 1024 * 1024);
    Status write(LogLevel level, LogEvent event, const QVariantMap &fields = {});
    void setLevel(LogLevel level) { QMutexLocker guard(&mutex_); level_ = level; }
    QString path() const { return path_; }
    static QVariantMap sanitized(const QVariantMap &fields);
private:
    QMutex mutex_;
    QString path_;
    qint64 maxBytes_ = 1024 * 1024;
    LogLevel level_ = LogLevel::Info;
};
}
