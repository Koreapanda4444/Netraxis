#pragma once
#include "core/Availability.h"
#include "core/TaskManager.h"
#include "domain/ConfigurationModels.h"
#include "domain/Evidence.h"
#include <variant>

namespace netraxis {
enum class BackendKind { Windows, Linux, MacOS };
enum class PlatformFeature { Interfaces, Addresses, Routes, Resolvers, Events, AddressWrite, RouteWrite, ResolverWrite };
struct PlatformDescriptor {
    BackendKind kind;
    QString name;
    QString implementation;
    QString hostIdentity;
};
struct AddressConfiguration {
    QString interfaceNativeId;
    QVector<InterfaceAddress> addresses;
};
struct RouteConfiguration {
    QString interfaceNativeId;
    QVector<Route> routes;
};
struct ResolverSettings {
    QString interfaceNativeId;
    QVector<IpAddress> servers;
    QStringList searchDomains;
};
using PlatformChangeValue = std::variant<AddressConfiguration, RouteConfiguration, ResolverSettings>;
struct PlatformChange {
    QUuid id;
    QString targetHost;
    PlatformChangeValue value;
    PlatformFeature feature() const;
    QString interfaceNativeId() const;
    Status validate() const;
};
class PrivilegePermit;
class INetworkPlatform {
public:
    virtual ~INetworkPlatform() = default;
    virtual PlatformDescriptor descriptor() const = 0;
    virtual FeatureState featureState(PlatformFeature feature) const = 0;
    virtual Result<QVector<Observation<NetworkInterface>>> interfaces(const TaskContext &context) = 0;
    virtual Result<QVector<Observation<InterfaceAddress>>> addresses(const QString &nativeId, const TaskContext &context) = 0;
    virtual Result<QVector<Observation<Route>>> routes(const TaskContext &context) = 0;
    virtual Result<QVector<Observation<ResolverConfiguration>>> resolvers(const TaskContext &context) = 0;
    virtual Status apply(const PlatformChange &change, const PrivilegePermit &permit, const TaskContext &context) = 0;
};
QString platformFeatureName(PlatformFeature feature);
}
