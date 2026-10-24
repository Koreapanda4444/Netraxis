#include "PortableNetworkPlatform.h"
#include <QNetworkInterface>
#include <QRegularExpression>

namespace netraxis {
namespace {
Error unavailable(){return {ErrorCode::Unsupported,"This native configuration provider has not been implemented for this build.",false,"platform"};}
QUuid identity(const QString &key){return QUuid::createUuidV5(QUuid("69ddc6df-fb83-4dc6-a1a8-ecb382791214"),key.toUtf8());}
InterfaceKind kind(QNetworkInterface::InterfaceType type){
    switch(type){case QNetworkInterface::Ethernet:return InterfaceKind::Ethernet;case QNetworkInterface::Wifi:return InterfaceKind::Wireless;case QNetworkInterface::Loopback:return InterfaceKind::Loopback;case QNetworkInterface::Virtual:return InterfaceKind::Virtual;case QNetworkInterface::Ppp:return InterfaceKind::Tunnel;default:return InterfaceKind::Unknown;}
}
}
PortableNetworkPlatform::PortableNetworkPlatform(PlatformDescriptor descriptor):descriptor_(std::move(descriptor)){}
FeatureState PortableNetworkPlatform::featureState(PlatformFeature feature) const {
    if(feature==PlatformFeature::Interfaces||feature==PlatformFeature::Addresses)return {Availability::ReadOnly,Verification::Unverified,"Available through Qt's native interface provider. No configuration writes are supported."};
    return {Availability::Unsupported,Verification::Unverified,unavailable().message};
}
Result<QVector<Observation<NetworkInterface>>> PortableNetworkPlatform::interfaces(const TaskContext &context){
    auto ready=context.check();if(!ready)return Result<QVector<Observation<NetworkInterface>>>::failure(ready.error());
    QVector<Observation<NetworkInterface>> values;
    const auto observed=QDateTime::currentDateTimeUtc();
    const auto nativeInterfaces=QNetworkInterface::allInterfaces();
    if(nativeInterfaces.isEmpty())return Result<QVector<Observation<NetworkInterface>>>::failure({ErrorCode::Network,"The native interface inventory is unavailable or access was denied. An empty result is not a verified network state.",true,"platform"});
    for(const auto &native:nativeInterfaces){
        ready=context.check();if(!ready)return Result<QVector<Observation<NetworkInterface>>>::failure(ready.error());
        if(!native.isValid())continue;
        NetworkInterface value;value.nativeId=QString::number(native.index());value.hostId=identity(descriptor_.hostIdentity);
        value.id=identity(descriptor_.hostIdentity+"/"+value.nativeId+"/"+native.name());
        value.name=native.humanReadableName().isEmpty()?native.name():native.humanReadableName();value.kind=kind(native.type());
        value.operational=native.flags().testFlag(QNetworkInterface::IsRunning);
        if(native.maximumTransmissionUnit()>0)value.mtu=quint32(native.maximumTransmissionUnit());
        const auto hardware=native.hardwareAddress().remove(':').remove('-');
        static const QRegularExpression hex("^(?:[a-fA-F0-9]{2}){1,32}$");
        if(hex.match(hardware).hasMatch())value.hardwareAddress=QByteArray::fromHex(hardware.toLatin1());
        for(const auto &entry:native.addressEntries()){
            const auto address=IpAddress::parse(entry.ip().toString());
            if(!address||entry.prefixLength()<0||entry.prefixLength()>address.value().bitCount())continue;
            value.addresses.push_back({address.value(),entry.prefixLength(),AddressOrigin::Unknown,{},{}});
        }
        const auto valid=value.validate();if(!valid)return Result<QVector<Observation<NetworkInterface>>>::failure(valid.error());
        Evidence evidence{QUuid::createUuid(),value.id,EvidenceKind::Observed,"Qt.QNetworkInterface",descriptor_.name,observed,{},10000,Availability::ReadOnly,{}};
        values.push_back({value,evidence});
    }
    ready=context.check();if(!ready)return Result<QVector<Observation<NetworkInterface>>>::failure(ready.error());
    return Result<QVector<Observation<NetworkInterface>>>::success(values);
}
Result<QVector<Observation<InterfaceAddress>>> PortableNetworkPlatform::addresses(const QString &nativeId,const TaskContext &context){
    auto snapshot=interfaces(context);if(!snapshot)return Result<QVector<Observation<InterfaceAddress>>>::failure(snapshot.error());
    for(const auto &interface:snapshot.value())if(interface.value.nativeId==nativeId){
        QVector<Observation<InterfaceAddress>> values;
        for(const auto &address:interface.value.addresses)values.push_back({address,interface.evidence});
        return Result<QVector<Observation<InterfaceAddress>>>::success(values);
    }
    return Result<QVector<Observation<InterfaceAddress>>>::failure({ErrorCode::Conflict,"The requested interface no longer exists.",true,"platform"});
}
Result<QVector<Observation<Route>>> PortableNetworkPlatform::routes(const TaskContext &context){
    const auto ready=context.check();return Result<QVector<Observation<Route>>>::failure(ready?unavailable():ready.error());
}
Result<QVector<Observation<ResolverConfiguration>>> PortableNetworkPlatform::resolvers(const TaskContext &context){
    const auto ready=context.check();return Result<QVector<Observation<ResolverConfiguration>>>::failure(ready?unavailable():ready.error());
}
Status PortableNetworkPlatform::apply(const PlatformChange &change,const PrivilegePermit &,const TaskContext &context){
    const auto ready=context.check();if(!ready)return ready;
    const auto valid=change.validate();if(!valid)return valid;
    return Status::failure(unavailable());
}
}
