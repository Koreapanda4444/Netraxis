#pragma once
#include "Capabilities.h"
#include <QSet>
#include <mutex>

namespace netraxis {
template<class T> struct PlatformSection {
    FeatureState state{Availability::Unknown,Verification::Unverified,{}};
    std::optional<QVector<Observation<T>>> observations;
    std::optional<Error> error;
};
struct PlatformSnapshot {
    CapabilityReport capabilities;
    QDateTime startedAt;
    QDateTime completedAt;
    PlatformSection<NetworkInterface> interfaces;
    PlatformSection<Route> routes;
    PlatformSection<ResolverConfiguration> resolvers;
};
class PlatformCoordinator final : public QObject {
    Q_OBJECT
public:
    using Operation=std::function<TaskResult(INetworkPlatform &,const TaskContext &)>;
    PlatformCoordinator(TaskManager &tasks,std::shared_ptr<INetworkPlatform> platform,QObject *parent=nullptr);
    ~PlatformCoordinator() override;
    Result<quint64> refresh(int timeoutMs=10000);
    Result<quint64> submit(Operation operation,int timeoutMs=10000);
    void cancel(quint64 id);
    void cancelRefresh();
    void stop();
signals:
    void operationFinished(quint64 id, netraxis::TaskResult result);
    void snapshotReady(netraxis::PlatformSnapshot snapshot);
    void failed(netraxis::Error error);
    void refreshingChanged(bool refreshing);
private:
    TaskManager &tasks_;
    std::shared_ptr<INetworkPlatform> platform_;
    std::shared_ptr<std::timed_mutex> gate_;
    QSet<quint64> operations_;
    quint64 refresh_ = 0;
    bool stopped_ = false;
};
}
Q_DECLARE_METATYPE(netraxis::PlatformSnapshot)
Q_DECLARE_METATYPE(netraxis::Error)
