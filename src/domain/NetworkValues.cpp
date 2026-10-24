#include "NetworkValues.h"
#include <QRegularExpression>

namespace netraxis {
namespace {
Error invalid(const QString &message) { return {ErrorCode::InvalidInput, message, false, "network_value"}; }
}
Result<IpAddress> IpAddress::parse(const QString &text) {
    QHostAddress address;
    if(text.size()>256)return Result<IpAddress>::failure(invalid("Address is too long."));
    for(auto c:text)if(c.isSpace()||c.isNull()||c.category()==QChar::Other_Control)
        return Result<IpAddress>::failure(invalid("Address contains invalid characters."));
    if (text != text.trimmed() || text.isEmpty() || !address.setAddress(text))
        return Result<IpAddress>::failure(invalid("A numeric IPv4 or IPv6 address is required."));
    static const QRegularExpression ipv4("^[0-9]{1,3}(\\.[0-9]{1,3}){3}$");
    if (address.protocol() == QAbstractSocket::IPv4Protocol && !ipv4.match(text).hasMatch())
        return Result<IpAddress>::failure(invalid("IPv4 addresses require four decimal octets."));
    if(address.protocol()==QAbstractSocket::IPv4Protocol) {
        quint32 value=0;
        for(const auto &part:text.split('.')) {
            bool valid=false;const auto octet=part.toUInt(&valid,10);
            if(!valid||octet>255||(part.size()>1&&part.startsWith('0')))
                return Result<IpAddress>::failure(invalid("IPv4 octets must be unambiguous decimal values."));
            value=(value<<8)|octet;
        }
        address.setAddress(value);
    }
    if (address.protocol() != QAbstractSocket::IPv4Protocol && address.protocol() != QAbstractSocket::IPv6Protocol)
        return Result<IpAddress>::failure(invalid("Unsupported address family."));
    return Result<IpAddress>::success(IpAddress(address));
}
Result<IpSubnet> IpSubnet::parse(const QString &cidr) {
    const auto parts = cidr.split('/');
    if (parts.size() != 2 || parts[1].isEmpty() || parts[0].contains('%'))
        return Result<IpSubnet>::failure(invalid("A numeric address and CIDR prefix are required."));
    auto address = IpAddress::parse(parts[0]);
    static const QRegularExpression digits("^[0-9]{1,3}$");
    bool ok = false;const auto prefix = parts[1].toInt(&ok);
    if (!address || !ok || !digits.match(parts[1]).hasMatch() || prefix < 0 || prefix > address.value().bitCount())
        return Result<IpSubnet>::failure(invalid("The subnet prefix does not match its address family."));
    auto network = address.value().native();
    if (address.value().isV4()) {
        const quint32 mask = prefix == 0 ? 0 : 0xffffffffu << (32 - prefix);
        network.setAddress(network.toIPv4Address() & mask);
    } else {
        auto bytes = network.toIPv6Address();
        for (int i = 0; i < 16; ++i) {
            const int bits = qBound(0, prefix - i * 8, 8);
            bytes[i] = static_cast<quint8>(bytes[i] & (bits == 0 ? 0 : (0xffu << (8 - bits))));
        }
        network.setAddress(bytes);
    }
    return Result<IpSubnet>::success(IpSubnet(IpAddress::parse(network.toString()).value(), prefix));
}
bool IpSubnet::contains(const IpAddress &address) const {
    return address.isV4() == network_.isV4() && address.native().isInSubnet(network_.native(), prefix_);
}
Result<Endpoint> Endpoint::create(IpAddress address, quint32 port, Transport transport) {
    if (port == 0 || port > 65535 || (transport != Transport::Tcp && transport != Transport::Udp))
        return Result<Endpoint>::failure(invalid("A TCP/UDP port between 1 and 65535 is required."));
    return Result<Endpoint>::success(Endpoint(std::move(address), static_cast<quint16>(port), transport));
}
QString Endpoint::toString() const {
    const auto host = address_.isV4() ? address_.toString() : '[' + address_.toString() + ']';
    return (transport_ == Transport::Tcp ? "tcp://" : "udp://") + host + ':' + QString::number(port_);
}
}
