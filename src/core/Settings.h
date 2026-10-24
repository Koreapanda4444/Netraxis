#pragma once
#include "Logger.h"
#include "Result.h"
#include <QByteArray>

namespace netraxis {
struct RuntimeSettings {
    int workerLimit = 4;
    int operationTimeoutMs = 10000;
    int retentionDays = 30;
    int storageLimitMiB = 128;
    LogLevel logLevel = LogLevel::Info;
    QByteArray windowGeometry;
    Status validate() const;
};
class SettingsStore final {
public:
    Result<RuntimeSettings> load() const;
    Status save(const RuntimeSettings &settings) const;
    static Result<QString> filePath();
};
}
