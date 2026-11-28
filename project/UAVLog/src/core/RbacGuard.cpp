#include "core/RbacGuard.h"
#include <stdexcept>

namespace uavlog {

bool RbacGuard::hasPermission(UserRole role, Permission perm) noexcept {
    // Все аутентифицированные роли наследуют право просмотра парка и формуляров (обобщение актёров)
    if (perm == Permission::ViewFleetAndFormular) {
        return true;
    }

    switch (role) {
        case UserRole::Administrator:
            return true; // Начальник службы: полный доступ (SRS 2.4)

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
        throw std::runtime_error("Отказ доступа (RBAC): роль «" + toRussianRole(role) +
                                 "» не имеет прав на выполнение данной операции");
    }
}

} // namespace uavlog
