#pragma once

#include "core/DomainTypes.h"

namespace uavlog {

class RbacGuard {
public:
    static bool hasPermission(UserRole role, Permission perm) noexcept;
    static void requirePermission(UserRole role, Permission perm);
};

} // namespace uavlog
