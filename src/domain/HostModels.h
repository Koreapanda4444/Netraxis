#pragma once
#include "NetworkValues.h"
#include <QDateTime>
#include <QUuid>
#include <QVector>

namespace netraxis {
Status validateEntityName(const QString &name);
struct NetworkWorkspace {
    QUuid id;
    QString name;
    QDateTime createdAt;
    Status validate() const;
};
struct Host {
    QUuid id;
    QString name;
    QString platform;
    QString nativeId;
    Status validate() const;
};
enum class InterfaceKind { Unknown, Ethernet, Wireless, Loopback, Tunnel, Virtual };
enum class AddressOrigin { Unknown, Static, Dhcp, Slaac };
struct InterfaceAddress {
    std::optional<IpAddress> address;
    int prefix = -1;
    AddressOrigin origin = AddressOrigin::Unknown;
    QDateTime preferredUntil;
    QDateTime validUntil;
    Status validate() const;
};
struct NetworkInterface {
    QUuid id;
    QUuid hostId;
    QString nativeId;
    QString name;
    InterfaceKind kind = InterfaceKind::Unknown;
    std::optional<bool> operational;
    std::optional<quint32> mtu;
    QByteArray hardwareAddress;
    QVector<InterfaceAddress> addresses;
    Status validate() const;
};
struct Route {
    QUuid id;
    QUuid interfaceId;
    std::optional<IpSubnet> destination;
    std::optional<IpAddress> gateway;
    std::optional<quint32> metric;
    QString nativeTable;
    Status validate() const;
};
}
