#include "DeviceModels.h"
#include <QRegularExpression>

namespace netraxis {
namespace {
Status invalid(){return Status::failure({ErrorCode::InvalidInput,"Invalid device, service or firewall entity.",false,"domain"});}
}
Status RouterProfile::validate() const {
    static const QRegularExpression adapter("^[a-zA-Z0-9._-]{1,64}$");
    if(id.isNull()||workspaceId.isNull()||!managementUrl.isValid()||managementUrl.host().isEmpty()
        ||(managementUrl.scheme()!="http"&&managementUrl.scheme()!="https")||!managementUrl.userInfo().isEmpty()
        ||managementUrl.hasQuery()||managementUrl.hasFragment()||managementUrl.port(443)<1||managementUrl.port(443)>65535
        ||!adapter.match(adapterId).hasMatch()||(secretReference&&secretReference->isNull()))return invalid();
    return validateEntityName(name);
}
Status LanDevice::validate() const {
    if(id.isNull()||workspaceId.isNull()||hardwareAddress.size()>32||addresses.size()>256)return invalid();
    return alias.isEmpty()?success():validateEntityName(alias);
}
Status ListeningEndpoint::validate() const {
    if(id.isNull()||hostId.isNull()||!endpoint)return invalid();
    return success();
}
Status Service::validate() const {
    if(id.isNull()||hostId.isNull()||listeners.size()>4096)return invalid();
    const auto status=validateEntityName(name);if(!status)return status;
    for(const auto &listener:listeners)if(listener.hostId!=hostId||!listener.validate())return invalid();
    return success();
}
Status PortRange::validate() const {
    if(first==0||first>last||last>65535)return invalid();
    return success();
}
Status FirewallRule::validate() const {
    if(id.isNull()||ownerId.isNull()||nativeId.size()>1024||localPorts.size()>4096||(managedBy&&managedBy->isNull()))return invalid();
    if((direction!=RuleDirection::Inbound&&direction!=RuleDirection::Outbound)||(action!=RuleAction::Allow&&action!=RuleAction::Block))return invalid();
    if((transport&&*transport!=Transport::Tcp&&*transport!=Transport::Udp)||(!transport&&!localPorts.isEmpty()))return invalid();
    for(const auto &ports:localPorts)if(!ports.validate())return invalid();
    return validateEntityName(name);
}
Status PortMapping::validate() const {
    if(id.isNull()||routerId.isNull()||serviceId.isNull()||externalPort==0||externalPort>65535||!internalEndpoint||(managedBy&&managedBy->isNull()))return invalid();
    return success();
}
}
