#include "SystemInfo.h"
#include <QSysInfo>

namespace netraxis {
SystemInfo SystemInfo::current() {
    return {QSysInfo::prettyProductName(), QSysInfo::kernelVersion(), QSysInfo::currentCpuArchitecture()};
}
}
