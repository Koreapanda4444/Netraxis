#pragma once
#include "SqliteStore.h"
#include <QDateTime>

namespace netraxis {
struct RetentionPolicy {
    int days = 30;
    qint64 maxBytes = 128LL * 1024 * 1024;
};
struct MaintenanceReport {
    qint64 removedObservations = 0;
    qint64 liveBytes = 0;
    qint64 fileBytes = 0;
    bool withinBudget = true;
};
class Maintenance final {
public:
    static Result<MaintenanceReport> retain(SqliteStore &store, const RetentionPolicy &policy,
        const QDateTime &now = QDateTime::currentDateTimeUtc(), const std::function<bool()> &cancelled = {});
    static Status clearSavedData(SqliteStore &store);
    static Result<qint64> liveBytes(SqliteStore &store);
private:
    static Status compact(SqliteStore &store);
};
}
