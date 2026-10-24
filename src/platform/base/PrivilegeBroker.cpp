#include "PrivilegeBroker.h"
#include <QCryptographicHash>
#include <QDataStream>
#include <QIODevice>

namespace netraxis {
namespace {
Status denied(const QString &message){return Status::failure({ErrorCode::PermissionDenied,message,false,"authorization"});}
}
Result<QByteArray> changeFingerprint(const PlatformChange &change){
    auto valid=change.validate();if(!valid)return Result<QByteArray>::failure(valid.error());
    QByteArray bytes;QDataStream out(&bytes,QIODevice::WriteOnly);out.setVersion(QDataStream::Qt_6_0);
    out<<quint32(1)<<change.id<<change.targetHost<<qint32(change.feature())<<change.interfaceNativeId();
    std::visit([&](const auto &value){
        using T=std::decay_t<decltype(value)>;
        if constexpr(std::is_same_v<T,AddressConfiguration>){
            out<<qint32(value.addresses.size());
            for(const auto &address:value.addresses)out<<address.address->toString()<<qint32(address.prefix)<<qint32(address.origin)<<address.preferredUntil.toUTC()<<address.validUntil.toUTC();
        }else if constexpr(std::is_same_v<T,RouteConfiguration>){
            out<<qint32(value.routes.size());
            for(const auto &route:value.routes){out<<route.id<<route.interfaceId<<route.destination->toString()<<bool(route.gateway);if(route.gateway)out<<route.gateway->toString();out<<bool(route.metric);if(route.metric)out<<*route.metric;out<<route.nativeTable;}
        }else{
            out<<qint32(value.servers.size());for(const auto &server:value.servers)out<<server.toString();out<<value.searchDomains;
        }
    },change.value);
    if(out.status()!=QDataStream::Ok)return Result<QByteArray>::failure({ErrorCode::Internal,"Cannot encode the approval request.",false,"authorization"});
    return Result<QByteArray>::success(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256));
}
Result<PrivilegeReview> PrivilegeBroker::review(const PlatformChange &change,const INetworkPlatform &platform) const {
    const auto digest=changeFingerprint(change);if(!digest)return Result<PrivilegeReview>::failure(digest.error());
    if(change.targetHost!=platform.descriptor().hostIdentity)return Result<PrivilegeReview>::failure({ErrorCode::PermissionDenied,"The request targets another host.",false,"authorization"});
    const auto capability=evaluateCapability(change.feature(),platform.featureState(change.feature()),currentPermissions());
    return Result<PrivilegeReview>::success({capability,digest.value(),platformFeatureName(change.feature())+" on interface "+change.interfaceNativeId()+". Network connectivity may be interrupted."});
}
Result<PrivilegePermit> PrivilegeBroker::authorize(const PlatformChange &change,const INetworkPlatform &platform,bool userConfirmed,int lifetimeMs) const {
    auto reviewed=review(change,platform);if(!reviewed)return Result<PrivilegePermit>::failure(reviewed.error());
    if(!userConfirmed)return Result<PrivilegePermit>::failure({ErrorCode::PermissionDenied,"Explicit user approval is required.",false,"authorization"});
    if(lifetimeMs<1||lifetimeMs>60000)return Result<PrivilegePermit>::failure({ErrorCode::InvalidInput,"Invalid approval lifetime.",false,"authorization"});
    if(!reviewed.value().capability.canWrite){
        const auto state=reviewed.value().capability.state;
        return Result<PrivilegePermit>::failure({state.availability==Availability::Unsupported||state.availability==Availability::ReadOnly?ErrorCode::Unsupported:ErrorCode::PermissionDenied,state.reason,false,"authorization"});
    }
    auto grant=std::make_shared<PrivilegePermit::Grant>();grant->fingerprint=reviewed.value().fingerprint;grant->host=change.targetHost;grant->expiry=QDeadlineTimer(lifetimeMs);
    return Result<PrivilegePermit>::success(PrivilegePermit(std::move(grant)));
}
Status PrivilegePermit::check(const PlatformChange &change,const QString &host) const {
    if(!grant_||grant_->consumed.load()||grant_->expiry.hasExpired())return denied("The approval is expired or already used.");
    const auto digest=changeFingerprint(change);if(!digest)return Status::failure(digest.error());
    if(host!=grant_->host||change.targetHost!=host||digest.value()!=grant_->fingerprint)return denied("The approved host or request no longer matches.");
    return success();
}
Status PrivilegePermit::consume(const PlatformChange &change,const QString &host) const {
    auto valid=check(change,host);if(!valid)return valid;
    bool expected=false;
    if(!grant_->consumed.compare_exchange_strong(expected,true))return denied("The approval was already consumed.");
    return success();
}
}
