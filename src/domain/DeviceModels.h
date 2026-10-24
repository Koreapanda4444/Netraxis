#pragma once
#include "HostModels.h"
#include "core/Availability.h"
#include <QUrl>

namespace netraxis {
struct RouterProfile {
    QUuid id;
    QUuid workspaceId;
    QString name;
    QUrl managementUrl;
    QString adapterId;
    std::optional<QUuid> secretReference;
    Status validate() const;
};
struct LanDevice {
    QUuid id;
    QUuid workspaceId;
    QString alias;
    QByteArray hardwareAddress;
    QVector<IpAddress> addresses;
    QDateTime lastSeen;
    Status validate() const;
};
struct ListeningEndpoint {
    QUuid id;
    QUuid hostId;
    std::optional<Endpoint> endpoint;
    std::optional<quint64> processId;
    Status validate() const;
};
struct Service {
    QUuid id;
    QUuid hostId;
    QString name;
    QVector<ListeningEndpoint> listeners;
    Status validate() const;
};
struct PortRange {
    quint32 first = 0;
    quint32 last = 0;
    Status validate() const;
};
enum class RuleDirection { Inbound, Outbound };
enum class RuleAction { Allow, Block };
struct FirewallRule {
    QUuid id;
    QUuid ownerId;
    QString nativeId;
    QString name;
    RuleDirection direction = RuleDirection::Inbound;
    RuleAction action = RuleAction::Block;
    std::optional<Transport> transport;
    QVector<PortRange> localPorts;
    std::optional<IpSubnet> remoteSubnet;
    std::optional<QUuid> managedBy;
    FeatureState state;
    Status validate() const;
};
struct PortMapping {
    QUuid id;
    QUuid routerId;
    QUuid serviceId;
    quint32 externalPort = 0;
    std::optional<Endpoint> internalEndpoint;
    std::optional<QUuid> managedBy;
    FeatureState state;
    Status validate() const;
};
}
