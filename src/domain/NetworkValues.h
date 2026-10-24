#pragma once
#include "core/Result.h"
#include <QHostAddress>
#include <optional>

namespace netraxis {
enum class Transport { Tcp, Udp };
class IpAddress final {
public:
    static Result<IpAddress> parse(const QString &text);
    QString toString() const { return address_.toString(); }
    const QHostAddress &native() const { return address_; }
    bool isV4() const { return address_.protocol() == QAbstractSocket::IPv4Protocol; }
    int bitCount() const { return isV4() ? 32 : 128; }
    bool operator==(const IpAddress &other) const { return address_ == other.address_; }
private:
    explicit IpAddress(QHostAddress address) : address_(std::move(address)) {}
    QHostAddress address_;
};
class IpSubnet final {
public:
    static Result<IpSubnet> parse(const QString &cidr);
    bool contains(const IpAddress &address) const;
    const IpAddress &network() const { return network_; }
    int prefix() const { return prefix_; }
    QString toString() const { return network_.toString() + '/' + QString::number(prefix_); }
private:
    IpSubnet(IpAddress network, int prefix) : network_(std::move(network)), prefix_(prefix) {}
    IpAddress network_;
    int prefix_;
};
class Endpoint final {
public:
    static Result<Endpoint> create(IpAddress address, quint32 port, Transport transport);
    const IpAddress &address() const { return address_; }
    quint16 port() const { return port_; }
    Transport transport() const { return transport_; }
    QString toString() const;
private:
    Endpoint(IpAddress address, quint16 port, Transport transport) : address_(std::move(address)), port_(port), transport_(transport) {}
    IpAddress address_;
    quint16 port_;
    Transport transport_;
};
}
