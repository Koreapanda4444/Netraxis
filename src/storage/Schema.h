#pragma once
#include "SqliteStore.h"

namespace netraxis {
class Schema final {
public:
    static constexpr int CurrentVersion = 2;
    static Status migrate(SqliteStore &store);
    static Status checkIntegrity(SqliteStore &store);
};
}
