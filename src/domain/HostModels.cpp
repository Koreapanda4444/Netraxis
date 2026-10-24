#include "HostModels.h"

namespace netraxis {
namespace {
Status invalid() {return Status::failure({ErrorCode::InvalidInput,"Invalid network entity.",false,"domain"});}
}
Status validateEntityName(const QString &name) {
    if(name.trimmed().isEmpty() || name.size()>256)return invalid();
    for(auto c:name)if(c.isNull()||c.category()==QChar::Other_Control)return invalid();
    return success();
}
Status NetworkWorkspace::validate() const {
    if(id.isNull()||!createdAt.isValid())return invalid();
    return validateEntityName(name);
}
Status Host::validate() const {
    if(id.isNull()||platform.isEmpty()||nativeId.isEmpty()||nativeId.size()>1024)return invalid();
    return validateEntityName(name);
}
Status InterfaceAddress::validate() const {
    if(!address||prefix<0||prefix>address->bitCount())return invalid();
    if(preferredUntil.isValid()&&validUntil.isValid()&&preferredUntil>validUntil)return invalid();
    if(int(origin)<0||int(origin)>int(AddressOrigin::Slaac))return invalid();
    return success();
}
Status NetworkInterface::validate() const {
    if(id.isNull()||hostId.isNull()||nativeId.isEmpty()||nativeId.size()>1024||hardwareAddress.size()>32||(mtu&&*mtu==0))return invalid();
    if(int(kind)<0||int(kind)>int(InterfaceKind::Virtual))return invalid();
    auto status=validateEntityName(name);if(!status)return status;
    for(const auto &address:addresses){status=address.validate();if(!status)return status;}
    return success();
}
Status Route::validate() const {
    if(id.isNull()||interfaceId.isNull()||!destination||nativeTable.size()>256)return invalid();
    if(gateway&&gateway->isV4()!=destination->network().isV4())return invalid();
    return success();
}
}
