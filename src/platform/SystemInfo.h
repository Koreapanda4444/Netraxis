#pragma once
#include <QString>

namespace netraxis {
struct SystemInfo {
    QString os;
    QString kernel;
    QString architecture;
    static SystemInfo current();
};
}
