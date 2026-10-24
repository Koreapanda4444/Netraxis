#pragma once
#include "NetworkPlatform.h"

namespace netraxis {
class PortableNetworkPlatform final : public INetworkPlatform {
public:
    explicit PortableNetworkPlatform(PlatformDescriptor descriptor);
    PlatformDescriptor descriptor() const override {return descriptor_;}
    FeatureState featureState(PlatformFeature feature) const override;
    Result<QVector<Observation<NetworkInterface>>> interfaces(const TaskContext &context) override;
    Result<QVector<Observation<InterfaceAddress>>> addresses(const QString &nativeId,const TaskContext &context) override;
    Result<QVector<Observation<Route>>> routes(const TaskContext &context) override;
    Result<QVector<Observation<ResolverConfiguration>>> resolvers(const TaskContext &context) override;
    Status apply(const PlatformChange &change,const PrivilegePermit &permit,const TaskContext &context) override;
private:
    PlatformDescriptor descriptor_;
};
}
