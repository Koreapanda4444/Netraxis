#include "Capabilities.h"
#if defined(Q_OS_WIN)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace netraxis {
PermissionState currentPermissions(){
#if defined(Q_OS_WIN)
    HANDLE token=nullptr;
    if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token))return {PrivilegeLevel::Unknown,false,Error{ErrorCode::PermissionDenied,"Cannot inspect the current process token.",false,"permissions"}};
    TOKEN_ELEVATION elevation{};DWORD size=0;
    const auto ok=GetTokenInformation(token,TokenElevation,&elevation,sizeof(elevation),&size);CloseHandle(token);
    if(!ok)return {PrivilegeLevel::Unknown,false,Error{ErrorCode::PermissionDenied,"Cannot inspect process elevation.",false,"permissions"}};
    return {elevation.TokenIsElevated?PrivilegeLevel::Elevated:PrivilegeLevel::Standard,false,{}};
#else
    return {geteuid()==0?PrivilegeLevel::Elevated:PrivilegeLevel::Standard,false,{}};
#endif
}
QVector<PlatformFeature> platformFeatures(){return {PlatformFeature::Interfaces,PlatformFeature::Addresses,PlatformFeature::Routes,PlatformFeature::Resolvers,PlatformFeature::Events,PlatformFeature::AddressWrite,PlatformFeature::RouteWrite,PlatformFeature::ResolverWrite};}
FeatureCapability evaluateCapability(PlatformFeature feature,const FeatureState &provider,const PermissionState &permissions){
    FeatureCapability result{feature,provider,false,false};
    const bool write=feature==PlatformFeature::AddressWrite||feature==PlatformFeature::RouteWrite||feature==PlatformFeature::ResolverWrite;
    if(write&&provider.availability==Availability::Supported&&permissions.level!=PrivilegeLevel::Elevated){
        result.state.availability=permissions.level==PrivilegeLevel::Unknown?Availability::Unknown:Availability::RequiresElevation;
        result.state.reason=permissions.level==PrivilegeLevel::Unknown?"Process permissions could not be verified.":"This operation requires administrator permission and explicit approval.";
    } else if(write&&provider.availability==Availability::RequiresElevation&&permissions.level==PrivilegeLevel::Elevated){
        result.state.availability=Availability::Supported;
        result.state.reason="Process elevation is present. Explicit approval is still required.";
    }
    const auto state=result.state.availability;
    result.canRead=!write&&(state==Availability::Supported||state==Availability::ReadOnly);
    result.canWrite=write&&state==Availability::Supported;
    return result;
}
CapabilityReport inspectCapabilities(const INetworkPlatform &platform){
    CapabilityReport report{platform.descriptor(),currentPermissions(),{}};
    for(auto feature:platformFeatures())report.features.push_back(evaluateCapability(feature,platform.featureState(feature),report.permissions));
    return report;
}
}
