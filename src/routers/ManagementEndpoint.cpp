#include "ManagementEndpoint.h"

namespace netraxis {
bool isValidManagementEndpoint(const QUrl &url) {
    return url.isValid() && !url.host().isEmpty()
        && (url.scheme() == "https" || url.scheme() == "http")
        && url.userInfo().isEmpty() && !url.hasFragment() && !url.hasQuery()
        && url.port(443) > 0 && url.port(443) <= 65535;
}
}
