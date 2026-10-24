#pragma once
#include "HostModels.h"
#include <QStringList>

namespace netraxis {
enum class ConfigurationSource { Unknown, OperatingSystem, Device, User };
enum class DnsTransport { Unknown, Plain, Tls, Https };
template<class T> struct ConfiguredObserved {
    std::optional<T> configured;
    std::optional<T> observed;
};
struct ResolverConfiguration {
    QUuid interfaceId;
    ConfigurationSource source = ConfigurationSource::Unknown;
    ConfiguredObserved<QVector<IpAddress>> servers;
    QStringList searchDomains;
    DnsTransport transport = DnsTransport::Unknown;
    Status validate() const;
};
enum class DhcpRole { HostClient, RouterServer };
struct DhcpConfiguration {
    QUuid ownerId;
    DhcpRole role = DhcpRole::HostClient;
    ConfiguredObserved<bool> enabled;
    std::optional<IpSubnet> poolSubnet;
    std::optional<IpAddress> server;
    Status validate() const;
};
struct DhcpLease {
    QUuid id;
    QUuid serverId;
    std::optional<IpAddress> address;
    QByteArray hardwareAddress;
    QString clientId;
    QDateTime expiresAt;
    Status validate() const;
};
struct WifiObservation {
    QUuid interfaceId;
    QByteArray ssid;
    QByteArray bssid;
    std::optional<int> signalPercent;
    std::optional<int> channel;
    std::optional<quint32> frequencyMHz;
    std::optional<quint64> linkBitsPerSecond;
    Status validate() const;
};
struct VpnLink {
    QUuid id;
    QUuid interfaceId;
    QString provider;
    ConfiguredObserved<bool> connected;
    QVector<IpSubnet> routes;
    QVector<IpAddress> resolvers;
    Status validate() const;
};
}
