#include "ConfigurationModels.h"
#include <QRegularExpression>
#include <QUrl>

namespace netraxis {
namespace {
Status invalid(){return Status::failure({ErrorCode::InvalidInput,"Invalid DNS, DHCP, wireless or VPN configuration.",false,"domain"});}
}
Status ResolverConfiguration::validate() const {
    if(interfaceId.isNull()||int(source)<0||int(source)>int(ConfigurationSource::User)||int(transport)<0||int(transport)>int(DnsTransport::Https))return invalid();
    if((servers.configured&&servers.configured->size()>64)||(servers.observed&&servers.observed->size()>64)||searchDomains.size()>64)return invalid();
    static const QRegularExpression label("^[a-zA-Z0-9](?:[a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?$");
    for(auto domain:searchDomains){
        if(domain.endsWith('.'))domain.chop(1);
        const auto ascii=QUrl::toAce(domain);if(ascii.isEmpty()||ascii.size()>253)return invalid();
        for(const auto &part:QString::fromLatin1(ascii).split('.'))if(!label.match(part).hasMatch())return invalid();
    }
    return success();
}
Status DhcpConfiguration::validate() const {
    if(ownerId.isNull()||(role!=DhcpRole::HostClient&&role!=DhcpRole::RouterServer))return invalid();
    if(role==DhcpRole::HostClient&&poolSubnet)return invalid();
    if(poolSubnet&&server&&poolSubnet->network().isV4()!=server->isV4())return invalid();
    return success();
}
Status DhcpLease::validate() const {
    if(id.isNull()||serverId.isNull()||!address||hardwareAddress.size()>32||clientId.size()>1024)return invalid();
    return success();
}
Status WifiObservation::validate() const {
    if(interfaceId.isNull()||ssid.size()>32||(!bssid.isEmpty()&&bssid.size()!=6)
        ||(signalPercent&&(*signalPercent<0||*signalPercent>100))||(channel&&(*channel<1||*channel>255))
        ||(frequencyMHz&&*frequencyMHz==0))return invalid();
    return success();
}
Status VpnLink::validate() const {
    if(id.isNull()||interfaceId.isNull()||provider.size()>256||routes.size()>4096||resolvers.size()>64)return invalid();
    return success();
}
}
