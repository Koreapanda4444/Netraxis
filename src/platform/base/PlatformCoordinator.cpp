#include "PlatformCoordinator.h"
#include <QThread>
#include <chrono>

namespace netraxis {
namespace {
template<class T,class Read> PlatformSection<T> section(INetworkPlatform &platform,PlatformFeature feature,const TaskContext &context,Read read){
    PlatformSection<T> value;value.state=platform.featureState(feature);
    if(!evaluateCapability(feature,value.state,currentPermissions()).canRead){value.state.verification=Verification::Skipped;return value;}
    auto ready=context.check();
    if(!ready){value.error=ready.error();value.state.verification=Verification::Failed;return value;}
    auto result=read();
    if(result){
        value.observations=std::move(result.value());value.state.verification=Verification::Healthy;
        value.state.reason="Collected from the local provider. This does not establish internet or service reachability.";
        for(const auto &observation:*value.observations)if(observation.evidence.errorCode){value.state.verification=Verification::Degraded;value.state.reason="Some native fields could not be normalized. Inspect observation evidence.";break;}
    }else{value.error=result.error();value.state.verification=Verification::Failed;value.state.reason=result.error().message;}
    return value;
}
}
PlatformCoordinator::PlatformCoordinator(TaskManager &tasks,std::shared_ptr<INetworkPlatform> platform,QObject *parent):QObject(parent),tasks_(tasks),platform_(std::move(platform)),gate_(std::make_shared<std::timed_mutex>()){
    qRegisterMetaType<PlatformSnapshot>();qRegisterMetaType<Error>();
    connect(&tasks_,&TaskManager::finished,this,[this](quint64 id,const TaskResult &result){
        if(!operations_.remove(id))return;
        emit operationFinished(id,result);
        if(id!=refresh_)return;
        refresh_=0;emit refreshingChanged(false);
        if(!result){emit failed(result.error());return;}
        if(!result.value().canConvert<PlatformSnapshot>()){emit failed({ErrorCode::Internal,"The backend returned an invalid snapshot.",false,"platform"});return;}
        emit snapshotReady(result.value().value<PlatformSnapshot>());
    });
}
PlatformCoordinator::~PlatformCoordinator(){stop();}
Result<quint64> PlatformCoordinator::submit(Operation operation,int timeoutMs){
    if(QThread::currentThread()!=thread()||!operation||!platform_||stopped_)return Result<quint64>::failure({ErrorCode::Conflict,"The platform coordinator is unavailable.",false,"platform"});
    const auto platform=platform_;const auto gate=gate_;
    auto job=tasks_.submit([platform,gate,operation=std::move(operation)](const TaskContext &context)->TaskResult{
        std::unique_lock<std::timed_mutex> lock(*gate,std::defer_lock);
        while(!lock.try_lock_for(std::chrono::milliseconds(10))){auto ready=context.check();if(!ready)return TaskResult::failure(ready.error());}
        auto ready=context.check();if(!ready)return TaskResult::failure(ready.error());
        return operation(*platform,context);
    },timeoutMs);
    if(job)operations_.insert(job.value());
    return job;
}
Result<quint64> PlatformCoordinator::refresh(int timeoutMs){
    if(refresh_)return Result<quint64>::failure({ErrorCode::Conflict,"A native refresh is already running.",true,"platform"});
    auto job=submit([](INetworkPlatform &platform,const TaskContext &context)->TaskResult{
        PlatformSnapshot snapshot;snapshot.startedAt=QDateTime::currentDateTimeUtc();snapshot.capabilities=inspectCapabilities(platform);
        snapshot.interfaces=section<NetworkInterface>(platform,PlatformFeature::Interfaces,context,[&]{return platform.interfaces(context);});
        snapshot.routes=section<Route>(platform,PlatformFeature::Routes,context,[&]{return platform.routes(context);});
        snapshot.resolvers=section<ResolverConfiguration>(platform,PlatformFeature::Resolvers,context,[&]{return platform.resolvers(context);});
        auto ready=context.check();if(!ready)return TaskResult::failure(ready.error());
        snapshot.completedAt=QDateTime::currentDateTimeUtc();return TaskResult::success(QVariant::fromValue(snapshot));
    },timeoutMs);
    if(job){refresh_=job.value();emit refreshingChanged(true);}
    return job;
}
void PlatformCoordinator::cancel(quint64 id){if(operations_.contains(id))tasks_.cancel(id);}
void PlatformCoordinator::cancelRefresh(){if(refresh_)tasks_.cancel(refresh_);}
void PlatformCoordinator::stop(){
    if(stopped_)return;
    stopped_=true;
    for(auto id:std::as_const(operations_))tasks_.cancel(id);
}
}
