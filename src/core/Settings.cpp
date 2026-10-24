#include "Settings.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <cmath>

namespace netraxis {
namespace {
Error invalid() { return {ErrorCode::InvalidInput, "Settings contain invalid values or an unsupported format.", false, "settings"}; }
Error storage() { return {ErrorCode::Storage, "Cannot access the local settings file.", false, "settings"}; }
}
Status RuntimeSettings::validate() const {
    if (workerLimit < 1 || workerLimit > 16 || operationTimeoutMs < 500 || operationTimeoutMs > 120000
        || retentionDays < 1 || retentionDays > 3650 || storageLimitMiB < 16 || storageLimitMiB > 4096
        || int(logLevel) < 0 || int(logLevel) > 3 || windowGeometry.size() > 65536)
        return Status::failure(invalid());
    return success();
}
Result<QString> SettingsStore::filePath() {
    const auto path = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (path.isEmpty() || !QDir().mkpath(path) || !QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner))
        return Result<QString>::failure(storage());
    return Result<QString>::success(path + "/settings.json");
}
Result<RuntimeSettings> SettingsStore::load() const {
    const auto path = filePath();
    if (!path) return Result<RuntimeSettings>::failure(path.error());
    if (!QFile::exists(path.value())) return Result<RuntimeSettings>::success({});
    QFile file(path.value());
    if (QFileInfo(file).isSymLink() || !file.open(QIODevice::ReadOnly) || file.size() > 128 * 1024)
        return Result<RuntimeSettings>::failure(storage());
    QJsonParseError parse;
    const auto doc = QJsonDocument::fromJson(file.readAll(), &parse);
    if (parse.error != QJsonParseError::NoError || !doc.isObject()) return Result<RuntimeSettings>::failure(invalid());
    const auto obj = doc.object();
    for (auto key : {"version", "workers", "timeout_ms", "retention_days", "storage_mib", "log_level"}) {
        const auto value = obj.value(QLatin1String(key));
        if (!value.isDouble() || !std::isfinite(value.toDouble()) || std::floor(value.toDouble()) != value.toDouble())
            return Result<RuntimeSettings>::failure(invalid());
    }
    if (obj.value("version").toInt(-1) != 1 || !obj.value("geometry").isString()) return Result<RuntimeSettings>::failure(invalid());
    const auto geometry = QByteArray::fromBase64Encoding(obj.value("geometry").toString().toLatin1(), QByteArray::AbortOnBase64DecodingErrors);
    if (!geometry) return Result<RuntimeSettings>::failure(invalid());
    RuntimeSettings settings;
    settings.workerLimit = obj.value("workers").toInt(-1);
    settings.operationTimeoutMs = obj.value("timeout_ms").toInt(-1);
    settings.retentionDays = obj.value("retention_days").toInt(-1);
    settings.storageLimitMiB = obj.value("storage_mib").toInt(-1);
    settings.logLevel = static_cast<LogLevel>(obj.value("log_level").toInt(-1));
    settings.windowGeometry = geometry.decoded;
    const auto valid = settings.validate();
    if (!valid) return Result<RuntimeSettings>::failure(valid.error());
    return Result<RuntimeSettings>::success(settings);
}
Status SettingsStore::save(const RuntimeSettings &settings) const {
    const auto valid = settings.validate();
    if (!valid) return valid;
    const auto path = filePath();
    if (!path) return Status::failure(path.error());
    if (QFileInfo(path.value()).isSymLink()) return Status::failure(storage());
    QSaveFile file(path.value());
    if (!file.open(QIODevice::WriteOnly) || !file.setPermissions(QFile::ReadOwner | QFile::WriteOwner)) return Status::failure(storage());
    const QJsonObject obj{{"version", 1}, {"workers", settings.workerLimit}, {"timeout_ms", settings.operationTimeoutMs},
        {"retention_days", settings.retentionDays}, {"storage_mib", settings.storageLimitMiB},
        {"log_level", int(settings.logLevel)}, {"geometry", QString::fromLatin1(settings.windowGeometry.toBase64())}};
    const auto bytes = QJsonDocument(obj).toJson(QJsonDocument::Indented);
    if (file.write(bytes) != bytes.size() || !file.commit()) return Status::failure(storage());
    return success();
}
}
