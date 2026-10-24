#pragma once
#include "Capabilities.h"

namespace netraxis {
Result<QByteArray> changeFingerprint(const PlatformChange &change);
struct PrivilegeReview {
    FeatureCapability capability;
    QByteArray fingerprint;
    QString summary;
};
class PrivilegePermit final {
public:
    Status check(const PlatformChange &change,const QString &host) const;
    Status consume(const PlatformChange &change,const QString &host) const;
private:
    struct Grant {
        QByteArray fingerprint;
        QString host;
        QDeadlineTimer expiry;
        std::atomic_bool consumed{false};
    };
    explicit PrivilegePermit(std::shared_ptr<Grant> grant):grant_(std::move(grant)){}
    std::shared_ptr<Grant> grant_;
    friend class PrivilegeBroker;
};
class PrivilegeBroker final {
public:
    Result<PrivilegeReview> review(const PlatformChange &change,const INetworkPlatform &platform) const;
    Result<PrivilegePermit> authorize(const PlatformChange &change,const INetworkPlatform &platform,bool userConfirmed,int lifetimeMs=30000) const;
};
}
