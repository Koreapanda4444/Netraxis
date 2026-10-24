#pragma once
#include "SqliteStore.h"
#include "domain/DeviceModels.h"
#include "domain/Evidence.h"

namespace netraxis {
class CoreRepository final {
public:
    explicit CoreRepository(SqliteStore &store) : store_(store) {}
    Result<QVector<NetworkWorkspace>> workspaces();
    Result<QVector<Host>> hosts();
    Result<QVector<NetworkInterface>> interfaces(const QUuid &hostId);
    Result<QVector<RouterProfile>> routers(const QUuid &workspaceId);
    Result<QVector<LanDevice>> devices(const QUuid &workspaceId);
    Status save(const NetworkWorkspace &value);
    Status save(const Host &value);
    Status save(const NetworkInterface &value);
    Status save(const RouterProfile &value);
    Status save(const LanDevice &value);
    Status removeWorkspace(const QUuid &id);
    Status removeHost(const QUuid &id);
    Status removeInterface(const QUuid &id);
    Status removeRouter(const QUuid &id);
    Status removeDevice(const QUuid &id);
    Status addEvidence(const Evidence &value);
    Result<QVector<Evidence>> evidence(const QUuid &subject, int limit = 100);
    Status transaction(const std::function<Status(CoreRepository &)> &body);
private:
    Status atomic(const std::function<Status()> &body);
    Status remove(const QString &table, const QUuid &id);
    SqliteStore &store_;
};
}
