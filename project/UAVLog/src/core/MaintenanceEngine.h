#pragma once

#include "core/DomainTypes.h"
#include <vector>

namespace uavlog {

class MaintenanceEngine {
public:
    static std::vector<MaintenanceRemainder> calculateRemainders(
        const Uav& uav,
        int commissionDayNumber,
        int currentDayNumber,
        const std::vector<MaintenanceRule>& rules,
        const std::vector<MaintenanceRecordSummary>& lastRecords,
        const AlertThresholds& thresholds = {});

    static std::vector<AlertItem> buildAlerts(
        const std::vector<Uav>& uavs,
        const std::vector<std::vector<MaintenanceRemainder>>& uavRemainders,
        const std::vector<OperatorInfo>& operators,
        const std::vector<Battery>& batteries,
        int currentDayNumber,
        const AlertThresholds& thresholds = {});
};

} // namespace uavlog
