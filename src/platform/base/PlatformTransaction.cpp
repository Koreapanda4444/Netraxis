#include "PlatformTransaction.h"
#include <algorithm>

namespace netraxis {
namespace {
Result<QByteArray> configurationFingerprint(PlatformChangeValue value){
    std::visit([](auto &configuration){
        using T=std::decay_t<decltype(configuration)>;
        if constexpr(std::is_same_v<T,AddressConfiguration>){
            std::sort(configuration.addresses.begin(),configuration.addresses.end(),[](const auto &a,const auto &b){return a.address->toString()+"/"+QString::number(a.prefix)<b.address->toString()+"/"+QString::number(b.prefix);});
        }else if constexpr(std::is_same_v<T,RouteConfiguration>){
            for(auto &route:configuration.routes)route.id=QUuid("00000000-0000-0000-0000-000000000001");
            const auto key=[](const Route &route){return route.destination->toString()+"/"+(route.gateway?route.gateway->toString():QString())+"/"+(route.metric?QString::number(*route.metric):QString())+"/"+route.nativeTable;};
            std::sort(configuration.routes.begin(),configuration.routes.end(),[&](const auto &a,const auto &b){return key(a)<key(b);});
        }
    },value);
    return changeFingerprint({QUuid("00000000-0000-0000-0000-000000000001"),"configuration",std::move(value)});
}
Error blocked(const QString &message){return {ErrorCode::Conflict,message,false,"platform_transaction"};}
Result<PlatformChangeValue> configuration(INetworkPlatform &platform,const PlatformChange &request,const TaskContext &context){
    auto ready=context.check();if(!ready)return Result<PlatformChangeValue>::failure(ready.error());
    auto result=platform.configuration(request.feature(),request.interfaceNativeId(),context);
    if(!result)return result;
    ready=context.check();if(!ready)return Result<PlatformChangeValue>::failure(ready.error());
    PlatformChange observed=request;observed.value=result.value();
    const auto valid=observed.validate();
    if(!valid||observed.feature()!=request.feature()||observed.interfaceNativeId()!=request.interfaceNativeId())
        return Result<PlatformChangeValue>::failure({ErrorCode::CorruptData,"The provider returned configuration for another target or type.",false,"platform_transaction"});
    return result;
}
Status matches(INetworkPlatform &platform,const PlatformChange &request,const TaskContext &context){
    auto actual=configuration(platform,request,context);if(!actual)return Status::failure(actual.error());
    auto expectedDigest=configurationFingerprint(request.value);auto actualDigest=configurationFingerprint(actual.value());
    if(!expectedDigest)return Status::failure(expectedDigest.error());
    if(!actualDigest)return Status::failure(actualDigest.error());
    if(expectedDigest.value()!=actualDigest.value())return Status::failure(blocked("The current configuration does not match the expected state."));
    return success();
}
Status validatePlan(const PreparedPlatformChange &plan){
    auto desired=plan.desired.validate();if(!desired)return desired;
    auto restore=plan.restore.validate();if(!restore)return restore;
    if(plan.desired.targetHost!=plan.restore.targetHost||plan.desired.interfaceNativeId()!=plan.restore.interfaceNativeId()||plan.desired.feature()!=plan.restore.feature())return Status::failure(blocked("The restoration request targets another operation."));
    auto before=configurationFingerprint(plan.restore.value);if(!before)return Status::failure(before.error());
    if(before.value()!=plan.beforeFingerprint)return Status::failure(blocked("The captured pre-change state was modified."));
    if(!plan.preparedAt.isValid()||plan.preparedAt.msecsTo(QDateTime::currentDateTimeUtc())<0||plan.preparedAt.msecsTo(QDateTime::currentDateTimeUtc())>60000)return Status::failure(blocked("The prepared change is stale. Capture and approve a new plan."));
    return success();
}
}
Result<PreparedPlatformChange> PlatformTransaction::prepare(INetworkPlatform &platform,const PlatformChange &desired,const TaskContext &context){
    PrivilegeBroker broker;auto reviewed=broker.review(desired,platform);if(!reviewed)return Result<PreparedPlatformChange>::failure(reviewed.error());
    if(!reviewed.value().capability.canWrite){
        const auto state=reviewed.value().capability.state;
        return Result<PreparedPlatformChange>::failure({state.availability==Availability::Unsupported||state.availability==Availability::ReadOnly?ErrorCode::Unsupported:ErrorCode::PermissionDenied,state.reason,false,"platform_transaction"});
    }
    auto before=configuration(platform,desired,context);if(!before)return Result<PreparedPlatformChange>::failure(before.error());
    auto fingerprint=configurationFingerprint(before.value());if(!fingerprint)return Result<PreparedPlatformChange>::failure(fingerprint.error());
    PlatformChange restore{QUuid::createUuid(),desired.targetHost,before.value()};
    return Result<PreparedPlatformChange>::success({desired,restore,fingerprint.value(),QDateTime::currentDateTimeUtc()});
}
Result<ApprovedPlatformChange> PlatformTransaction::authorize(const PreparedPlatformChange &plan,const INetworkPlatform &platform,bool userConfirmed){
    auto valid=validatePlan(plan);if(!valid)return Result<ApprovedPlatformChange>::failure(valid.error());
    PrivilegeBroker broker;auto apply=broker.authorize(plan.desired,platform,userConfirmed);if(!apply)return Result<ApprovedPlatformChange>::failure(apply.error());
    auto restore=broker.authorize(plan.restore,platform,userConfirmed);if(!restore)return Result<ApprovedPlatformChange>::failure(restore.error());
    return Result<ApprovedPlatformChange>::success({plan,apply.value(),restore.value()});
}
PlatformExecution PlatformTransaction::execute(INetworkPlatform &platform,const ApprovedPlatformChange &approved,const TaskContext &context){
    const auto &plan=approved.plan;
    auto valid=validatePlan(plan);if(!valid)return {PlatformChangeOutcome::Blocked,valid.error(),{}};
    valid=approved.applyPermit.check(plan.desired,platform.descriptor().hostIdentity);if(!valid)return {PlatformChangeOutcome::Blocked,valid.error(),{}};
    valid=approved.restorePermit.check(plan.restore,platform.descriptor().hostIdentity);if(!valid)return {PlatformChangeOutcome::Blocked,valid.error(),{}};
    PrivilegeBroker broker;auto review=broker.review(plan.desired,platform);
    if(!review||!review.value().capability.canWrite)return {PlatformChangeOutcome::Blocked,review?blocked(review.value().capability.state.reason):review.error(),{}};
    valid=matches(platform,plan.restore,context);if(!valid)return {PlatformChangeOutcome::Blocked,valid.error(),{}};
    valid=context.check();if(!valid)return {PlatformChangeOutcome::Blocked,valid.error(),{}};
    auto applied=platform.apply(plan.desired,approved.applyPermit,context);
    if(applied){valid=context.check();if(valid)valid=matches(platform,plan.desired,context);if(valid)return {PlatformChangeOutcome::Applied,{},{}};}
    const auto error=applied?valid.error():applied.error();
    const TaskContext recovery(std::make_shared<std::atomic_bool>(false),QDeadlineTimer(10000));
    auto authorized=approved.restorePermit.check(plan.restore,platform.descriptor().hostIdentity);
    if(!authorized)return {PlatformChangeOutcome::CompensationPending,error,authorized.error()};
    auto restored=platform.apply(plan.restore,approved.restorePermit,recovery);
    if(!restored)return {PlatformChangeOutcome::RestorationFailed,error,restored.error()};
    auto verified=matches(platform,plan.restore,recovery);
    if(!verified)return {PlatformChangeOutcome::RestorationFailed,error,verified.error()};
    return {PlatformChangeOutcome::Restored,error,{}};
}
}
