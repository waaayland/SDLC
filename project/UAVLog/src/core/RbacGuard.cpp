#include "core/RbacGuard.h"
#include <stdexcept>

namespace uavlog {

bool RbacGuard::hasPermission(UserRole role, Permission perm) noexcept {
    // Базовое право просмотра парка и формуляров есть у всех аутентифицированных ролей (обобщение актёров)
    if (perm == Permission::ViewFleetAndFormular) {
        return true;
    }

    switch (role) {
        case UserRole::Administrator:
            return true; // Полный доступ ко всем функциям (SRS 2.4)

        case UserRole::Technician:
            return perm == Permission::RegisterPreflightCheck ||
                   perm == Permission::RegisterMaintenance ||
                   perm == Permission::ReportFault ||
                   perm == Permission::ResolveFault;

        case UserRole::Operator:
            return perm == Permission::RegisterPreflightCheck ||
                   perm == Permission::RegisterFlight ||
                   perm == Permission::ReportFault;

        case UserRole::Observer:
            return perm == Permission::GenerateReports;
    }
    return false;
}

void RbacGuard::requirePermission(UserRole role, Permission perm) {
    if (!hasPermission(role, perm)) {
        throw std::runtime_error("Недостаточно прав роли «" + toRussianRole(role) +
                                 "» для выполнения операции (RBAC)");
    }
}

} // namespace uavlog
