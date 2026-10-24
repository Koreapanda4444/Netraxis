#pragma once
#include "NetworkPlatform.h"

namespace netraxis {
enum class PrivilegeLevel { Standard, Elevated, Unknown };
struct PermissionState {
    PrivilegeLevel level = PrivilegeLevel::Unknown;
    bool helperAvailable = false;
    std::optional<Error> error;
};
struct FeatureCapability {
    PlatformFeature feature;
    FeatureState state;
    bool canRead = false;
    bool canWrite = false;
};
struct CapabilityReport {
    PlatformDescriptor platform;
    PermissionState permissions;
    QVector<FeatureCapability> features;
};
PermissionState currentPermissions();
FeatureCapability evaluateCapability(PlatformFeature feature,const FeatureState &provider,const PermissionState &permissions);
CapabilityReport inspectCapabilities(const INetworkPlatform &platform);
QVector<PlatformFeature> platformFeatures();
}
