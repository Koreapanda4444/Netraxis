#include "NativeNormalization.h"
#include <QRegularExpression>
#include <QSet>

namespace netraxis {
namespace {
QUuid identity(const QString &key){return QUuid::createUuidV5(QUuid("69ddc6df-fb83-4dc6-a1a8-ecb382791214"),key.toUtf8());}
Error invalid(const QString &message){return {ErrorCode::InvalidInput,message,false,"normalization"};}
}
QUuid NativeNormalization::hostId(const QString &hostIdentity){return identity("host/"+hostIdentity);}
Result<NormalizedInterface> NativeNormalization::normalizeInterface(const NativeInterfaceRecord &native,const PlatformDescriptor &platform,const QDateTime &observedAt){
    if(native.nativeId.isEmpty()||native.nativeId.size()>1024||platform.hostIdentity.isEmpty()||!observedAt.isValid()||native.source.isEmpty()||native.source.size()>256)
        return Result<NormalizedInterface>::failure(invalid("Native interface identity or provenance is missing."));
    NetworkInterface value;value.nativeId=native.nativeId;value.hostId=hostId(platform.hostIdentity);
    value.name=native.name;value.kind=native.kind;value.operational=native.running;value.mtu=native.mtu;
    QVector<NormalizationIssue> issues;
    const auto issue=[&](const QString &field,const QString &message){issues.push_back({field,invalid(message)});};
    if(value.mtu&&*value.mtu==0){value.mtu.reset();issue("mtu","The native MTU is invalid.");}
    if(!native.hardwareAddress.isEmpty()){
        auto text=native.hardwareAddress;text.remove(':').remove('-');
        static const QRegularExpression hex("^(?:[a-fA-F0-9]{2}){1,32}$");
        if(!hex.match(text).hasMatch())issue("hardware_address","The native hardware address has an invalid format.");
        else value.hardwareAddress=QByteArray::fromHex(text.toLatin1());
    }
    const auto discriminator=value.hardwareAddress.isEmpty()?native.name:QString::fromLatin1(value.hardwareAddress.toHex());
    value.id=identity(platform.hostIdentity+"/interface/"+value.nativeId+"/"+discriminator);
    QSet<QString> seen;
    for(const auto &raw:native.addresses){
        auto parsed=IpAddress::parse(raw.address);
        if(!parsed){issue("address",parsed.error().message);continue;}
        auto address=parsed.value();
        if(!address.isV4()&&address.native().isLinkLocal()&&address.native().scopeId().isEmpty()){
            auto scoped=address.native();scoped.setScopeId(native.nativeId);
            auto parsedScope=IpAddress::parse(scoped.toString());
            if(!parsedScope){issue("address_scope",parsedScope.error().message);continue;}
            address=parsedScope.value();
        }
        InterfaceAddress assigned{address,raw.prefix,raw.origin,raw.preferredUntil,raw.validUntil};
        auto valid=assigned.validate();if(!valid){issue("address",valid.error().message);continue;}
        const auto key=address.toString()+"/"+QString::number(raw.prefix);if(seen.contains(key))continue;
        seen.insert(key);value.addresses.push_back(assigned);
    }
    const auto valid=value.validate();if(!valid)return Result<NormalizedInterface>::failure(valid.error());
    Evidence evidence{QUuid::createUuid(),value.id,EvidenceKind::Observed,native.source,platform.name,observedAt.toUTC(),{},10000,Availability::ReadOnly,{}};
    if(!issues.isEmpty())evidence.errorCode=ErrorCode::InvalidInput;
    const auto provenance=evidence.validate();if(!provenance)return Result<NormalizedInterface>::failure(provenance.error());
    return Result<NormalizedInterface>::success({{value,evidence},issues});
}
}
