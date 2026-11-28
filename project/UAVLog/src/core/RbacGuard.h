#pragma once

#include "core/DomainTypes.h"

namespace uavlog {

// Принудительная проверка прав ролевой модели (RBAC) в сервисном слое (REQ-SEC-002, ADR-0003)
class RbacGuard {
public:
    static bool hasPermission(UserRole role, Permission perm) noexcept;
    static void requirePermission(UserRole role, Permission perm);
};

} // namespace uavlog
