#pragma once
#include "NetworkPlatform.h"
#include <memory>

namespace netraxis {
BackendKind currentBackendKind();
Result<std::shared_ptr<INetworkPlatform>> createNetworkPlatform();
Result<std::shared_ptr<INetworkPlatform>> createNetworkPlatform(BackendKind requested);
}
