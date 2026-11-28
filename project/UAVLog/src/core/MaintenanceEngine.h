#pragma once

#include "core/DomainTypes.h"
#include <vector>

namespace uavlog {

// Таблично-управляемый движок расчёта остатков до ТО и предупреждений (ADR-0004, REQ-FUNC-008/009-2)
class MaintenanceEngine {
public:
    static std::vector<MaintenanceRemainder> calculateRemainders(
        const Uav& uav,
        int commissionDayNumber,
        int todayDayNumber,
        const std::vector<MaintenanceRule>& rules,
        const std::vector<MaintenanceRecordSummary>& maintRecords,
        const AlertThresholds& thresholds = {});

    static std::vector<AlertItem> buildAlerts(
        const std::vector<Uav>& uavs,
        const std::vector<std::vector<MaintenanceRemainder>>& uavRemainders,
        const std::vector<OperatorInfo>& operators,
        const std::vector<Battery>& batteries,
        int todayDayNumber,
        const AlertThresholds& thresholds = {});
};

} // namespace uavlog
