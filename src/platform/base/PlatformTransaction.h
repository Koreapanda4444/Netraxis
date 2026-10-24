#pragma once
#include "PrivilegeBroker.h"

namespace netraxis {
struct PreparedPlatformChange {
    PlatformChange desired;
    PlatformChange restore;
    QByteArray beforeFingerprint;
    QDateTime preparedAt;
};
struct ApprovedPlatformChange {
    PreparedPlatformChange plan;
    PrivilegePermit applyPermit;
    PrivilegePermit restorePermit;
};
enum class PlatformChangeOutcome { Blocked, Applied, Restored, CompensationPending, RestorationFailed };
struct PlatformExecution {
    PlatformChangeOutcome outcome = PlatformChangeOutcome::Blocked;
    std::optional<Error> error;
    std::optional<Error> restorationError;
};
class PlatformTransaction final {
public:
    static Result<PreparedPlatformChange> prepare(INetworkPlatform &platform,const PlatformChange &desired,const TaskContext &context);
    static Result<ApprovedPlatformChange> authorize(const PreparedPlatformChange &plan,const INetworkPlatform &platform,bool userConfirmed);
    static PlatformExecution execute(INetworkPlatform &platform,const ApprovedPlatformChange &approved,const TaskContext &context);
};
}
Q_DECLARE_METATYPE(netraxis::PlatformExecution)
