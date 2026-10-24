#pragma once
#include "NetworkPlatform.h"

namespace netraxis {
struct NativeAddressRecord {
    QString address;
    int prefix = -1;
    AddressOrigin origin = AddressOrigin::Unknown;
    QDateTime preferredUntil;
    QDateTime validUntil;
};
struct NativeInterfaceRecord {
    QString nativeId;
    QString name;
    QString hardwareAddress;
    InterfaceKind kind = InterfaceKind::Unknown;
    std::optional<bool> running;
    std::optional<quint32> mtu;
    QVector<NativeAddressRecord> addresses;
    QString source;
};
struct NormalizationIssue {
    QString field;
    Error error;
};
struct NormalizedInterface {
    Observation<NetworkInterface> observation;
    QVector<NormalizationIssue> issues;
};
class NativeNormalization final {
public:
    static Result<NormalizedInterface> normalizeInterface(const NativeInterfaceRecord &native,const PlatformDescriptor &platform,const QDateTime &observedAt);
    static QUuid hostId(const QString &hostIdentity);
};
}
