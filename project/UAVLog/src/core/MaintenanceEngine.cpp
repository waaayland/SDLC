#include "core/MaintenanceEngine.h"

#include <cmath>
#include <sstream>
#include <iomanip>

namespace uavlog {

std::vector<MaintenanceRemainder> MaintenanceEngine::calculateRemainders(
    const Uav& uav,
    int commissionDayNumber,
    int currentDayNumber,
    const std::vector<MaintenanceRule>& rules,
    const std::vector<MaintenanceRecordSummary>& lastRecords,
    const AlertThresholds& thresholds) {

    std::vector<MaintenanceRemainder> out;
    out.reserve(rules.size());

    for (const auto& rule : rules) {
        if (rule.uavTypeId != uav.uavTypeId) {
            continue;
        }
        int baseFlightMin = 0;
        int baseDay = commissionDayNumber;
        for (const auto& rec : lastRecords) {
            if (rec.ruleId == rule.id) {
                if (rec.performedDayNumber >= baseDay) {
                    baseDay = rec.performedDayNumber;
                    baseFlightMin = rec.flightMinAtMaint;
                }
            }
        }

        MaintenanceRemainder rem;
        rem.ruleId = rule.id;
        rem.ruleTitle = rule.title;

        if (rule.intervalHours.has_value() && *rule.intervalHours > 0.0) {
            const double usedHours = static_cast<double>(uav.totalFlightMin - baseFlightMin) / 60.0;
            const double leftHours = *rule.intervalHours - usedHours;
            rem.remainingHours = leftHours;
            if (leftHours <= 0.0) {
                rem.isOverdue = true;
            } else if (leftHours <= (*rule.intervalHours * thresholds.maintHoursWarnRatio) + 1e-9) {
                rem.isWarning = true;
            }
        }

        if (rule.intervalDays.has_value() && *rule.intervalDays > 0) {
            const int dueDay = baseDay + *rule.intervalDays;
            const int leftDays = dueDay - currentDayNumber;
            rem.remainingDays = leftDays;
            if (leftDays <= 0) {
                rem.isOverdue = true;
            } else if (leftDays <= thresholds.maintDaysWarn) {
                rem.isWarning = true;
            }
        }

        out.push_back(rem);
    }
    return out;
}

std::vector<AlertItem> MaintenanceEngine::buildAlerts(
    const std::vector<Uav>& uavs,
    const std::vector<std::vector<MaintenanceRemainder>>& uavRemainders,
    const std::vector<OperatorInfo>& operators,
    const std::vector<Battery>& batteries,
    int currentDayNumber,
    const AlertThresholds& thresholds) {

    std::vector<AlertItem> criticals;
    std::vector<AlertItem> warnings;

    // 1. Предупреждения по БЛА и ТО
    for (std::size_t i = 0; i < uavs.size() && i < uavRemainders.size(); ++i) {
        const auto& u = uavs[i];
        if (u.status == UavStatus::Retired) continue;

        if (u.status == UavStatus::Faulty) {
            criticals.push_back({AlertSeverity::Critical, "UAV", u.id,
                                 "Борт " + u.tailNumber + ": статус «Неисправен» (вылет запрещён)"});
        }
        for (const auto& rem : uavRemainders[i]) {
            if (rem.isOverdue) {
                criticals.push_back({AlertSeverity::Critical, "UAV", u.id,
                                     "Борт " + u.tailNumber + ": ПРОСРОЧЕНО ТО «" + rem.ruleTitle + "»"});
            } else if (rem.isWarning) {
                std::ostringstream oss;
                oss << "Борт " << u.tailNumber << ": приближается ТО «" << rem.ruleTitle << "» (";
                bool hasPart = false;
                if (rem.remainingHours.has_value()) {
                    oss << std::fixed << std::setprecision(1) << *rem.remainingHours << " ч";
                    hasPart = true;
                }
                if (rem.remainingDays.has_value()) {
                    if (hasPart) oss << " / ";
                    oss << *rem.remainingDays << " дн.";
                }
                oss << ")";
                warnings.push_back({AlertSeverity::Warning, "UAV", u.id, oss.str()});
            }
        }
    }

    // 2. Предупреждения по допускам операторов (порог 14 дней, REQ-FUNC-009-2)
    for (const auto& op : operators) {
        const int leftDays = op.permitExpiryDayNumber - currentDayNumber;
        if (leftDays < 0) {
            criticals.push_back({AlertSeverity::Critical, "OPERATOR", op.id,
                                 "Оператор " + op.fullName + ": допуск ИСТЁК (" + op.permitExpiryIso + ")"});
        } else if (leftDays <= thresholds.permitDaysWarn) {
            warnings.push_back({AlertSeverity::Warning, "OPERATOR", op.id,
                                "Оператор " + op.fullName + ": допуск истекает через " +
                                std::to_string(leftDays) + " дн."});
        }
    }

    // 3. Предупреждения по ресурсу АКБ (порог 90 %, блокировка 100 %, REQ-FUNC-016)
    for (const auto& bat : batteries) {
        if (bat.retired || bat.maxCycles <= 0) continue;
        const double ratio = static_cast<double>(bat.usedCycles) / static_cast<double>(bat.maxCycles);
        if (bat.usedCycles >= bat.maxCycles) {
            criticals.push_back({AlertSeverity::Critical, "BATTERY", bat.id,
                                 "АКБ " + bat.inventoryNumber + ": ресурс ИСЧЕРПАН (" +
                                 std::to_string(bat.usedCycles) + "/" + std::to_string(bat.maxCycles) + ")"});
        } else if (ratio >= thresholds.batteryWarnRatio - 1e-9) {
            const int pct = static_cast<int>(std::round(ratio * 100.0));
            warnings.push_back({AlertSeverity::Warning, "BATTERY", bat.id,
                                "АКБ " + bat.inventoryNumber + ": выработано " +
                                std::to_string(pct) + " % ресурса (" +
                                std::to_string(bat.usedCycles) + "/" + std::to_string(bat.maxCycles) + ")"});
        }
    }

    // Критические сначала, затем предупреждения
    criticals.insert(criticals.end(), warnings.begin(), warnings.end());
    return criticals;
}

} // namespace uavlog
