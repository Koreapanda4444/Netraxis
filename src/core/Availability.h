#pragma once
#include <QString>
#include "Result.h"

namespace netraxis {
enum class Availability { Supported, ReadOnly, RequiresAuth, RequiresElevation, RequiresEntitlement, Unsupported, Unknown };
enum class Verification { Healthy, Degraded, Failed, Unverified, Skipped };
struct FeatureState {
    Availability availability = Availability::Unknown;
    Verification verification = Verification::Unverified;
    QString reason;
    bool canWrite() const { return availability == Availability::Supported; }
};
inline QString availabilityLabel(Availability state) {
    switch(state) {
    case Availability::Supported: return "Supported";
    case Availability::ReadOnly: return "Read only";
    case Availability::RequiresAuth: return "Authentication required";
    case Availability::RequiresElevation: return "Administrator permission required";
    case Availability::RequiresEntitlement: return "Entitlement required";
    case Availability::Unsupported: return "Unsupported";
    case Availability::Unknown: return "Unknown";
    }
    return "Unknown";
}
}
