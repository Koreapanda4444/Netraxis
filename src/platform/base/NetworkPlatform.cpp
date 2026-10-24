#include "NetworkPlatform.h"
#include <QSet>

namespace netraxis {
PlatformFeature PlatformChange::feature() const {
    return std::visit([](const auto &change) {
        using T = std::decay_t<decltype(change)>;
        if constexpr(std::is_same_v<T, AddressConfiguration>) return PlatformFeature::AddressWrite;
        else if constexpr(std::is_same_v<T, RouteConfiguration>) return PlatformFeature::RouteWrite;
        else return PlatformFeature::ResolverWrite;
    },value);
}
QString PlatformChange::interfaceNativeId() const {
    return std::visit([](const auto &change){return change.interfaceNativeId;},value);
}
Status PlatformChange::validate() const {
    const auto invalid = []{return Status::failure({ErrorCode::InvalidInput,"Invalid platform configuration request.",false,"platform"});};
    if(id.isNull() || targetHost.isEmpty() || targetHost.size()>256 || interfaceNativeId().isEmpty() || interfaceNativeId().size()>1024) return invalid();
    return std::visit([&](const auto &change)->Status {
        using T = std::decay_t<decltype(change)>;
        if constexpr(std::is_same_v<T, AddressConfiguration>) {
            if(change.addresses.size()>4096)return invalid();
            QSet<QString> seen;
            for(const auto &address:change.addresses) {
                auto valid=address.validate();if(!valid)return valid;
                const auto key=address.address->toString()+"/"+QString::number(address.prefix);
                if(seen.contains(key))return invalid();
                seen.insert(key);
            }
        } else if constexpr(std::is_same_v<T, RouteConfiguration>) {
            if(change.routes.size()>4096)return invalid();
            for(const auto &route:change.routes){auto valid=route.validate();if(!valid)return valid;}
        } else {
            ResolverConfiguration resolver;
            resolver.interfaceId=id;resolver.servers.configured=change.servers;
            resolver.searchDomains=change.searchDomains;resolver.source=ConfigurationSource::User;
            return resolver.validate();
        }
        return success();
    },value);
}
QString platformFeatureName(PlatformFeature feature) {
    switch(feature) {
    case PlatformFeature::Interfaces:return "Interfaces";
    case PlatformFeature::Addresses:return "Addresses";
    case PlatformFeature::Routes:return "Routes";
    case PlatformFeature::Resolvers:return "DNS configuration";
    case PlatformFeature::Events:return "System network events";
    case PlatformFeature::AddressWrite:return "Address changes";
    case PlatformFeature::RouteWrite:return "Route changes";
    case PlatformFeature::ResolverWrite:return "DNS changes";
    }
    return "Unknown feature";
}
}
