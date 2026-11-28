#include "core/MaintenanceEngine.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace uavlog {

namespace {
std::string formatOneDecimal(double value) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << value;
    return oss.str();
}
} // namespace

std::vector<MaintenanceRemainder> MaintenanceEngine::calculateRemainders(
    const Uav& uav,
    int commissionDayNumber,
    int todayDayNumber,
    const std::vector<MaintenanceRule>& rules,
    const std::vector<MaintenanceRecordSummary>& maintRecords,
    const AlertThresholds& thresholds) {

    std::vector<MaintenanceRemainder> result;

    for (const auto& rule : rules) {
        if (rule.uavTypeId != uav.uavTypeId) {
            continue;
        }

        // Поиск последнего выполненного ТО данного вида (ADR-0004)
        int lastMaintFlightMin = 0;
        int lastMaintDay = commissionDayNumber;
        bool hasMaint = false;

        for (const auto& rec : maintRecords) {
            if (rec.ruleId != rule.id) continue;
            if (!hasMaint || rec.performedDayNumber > lastMaintDay ||
                (rec.performedDayNumber == lastMaintDay && rec.flightMinAtMaint > lastMaintFlightMin)) {
                hasMaint = true;
                lastMaintDay = rec.performedDayNumber;
                lastMaintFlightMin = rec.flightMinAtMaint;
            }
        }

        MaintenanceRemainder rem;
        rem.ruleId = rule.id;
        rem.ruleTitle = rule.title;

        if (rule.intervalHours.has_value() && *rule.intervalHours > 0.0) {
            const double usedHours = std::max(0, uav.totalFlightMin - lastMaintFlightMin) / 60.0;
            const double leftHours = *rule.intervalHours - usedHours;
            rem.remainingHours = leftHours;

            if (leftHours <= 0.0) {
                rem.isOverdue = true;
            } else if (leftHours <= (*rule.intervalHours) * thresholds.maintHoursWarnRatio + 1e-9) {
                rem.isWarning = true;
            }
        }

        if (rule.intervalDays.has_value() && *rule.intervalDays > 0) {
            const int leftDays = (lastMaintDay + *rule.intervalDays) - todayDayNumber;
            rem.remainingDays = leftDays;

            if (leftDays <= 0) {
                rem.isOverdue = true;
            } else if (leftDays <= thresholds.maintDaysWarn) {
                rem.isWarning = true;
            }
        }

        result.push_back(std::move(rem));
    }

    return result;
}

std::vector<AlertItem> MaintenanceEngine::buildAlerts(
    const std::vector<Uav>& uavs,
    const std::vector<std::vector<MaintenanceRemainder>>& uavRemainders,
    const std::vector<OperatorInfo>& operators,
    const std::vector<Battery>& batteries,
    int todayDayNumber,
    const AlertThresholds& thresholds) {

    std::vector<AlertItem> alerts;

    // 1. Борта: неисправности и сроки ТО (REQ-FUNC-009-2)
    for (std::size_t i = 0; i < uavs.size(); ++i) {
        const auto& u = uavs[i];
        if (u.status == UavStatus::Retired) continue;

        if (u.status == UavStatus::Faulty) {
            alerts.push_back({
                AlertSeverity::Critical,
                "Неисправность",
                "UAV",
                u.id,
                "Борт " + u.tailNumber + ": НЕИСПРАВЕН — имеются открытые критичные замечания"
            });
        }

        if (i < uavRemainders.size()) {
            for (const auto& r : uavRemainders[i]) {
                if (r.isOverdue) {
                    std::string detail;
                    if (r.remainingHours.has_value() && *r.remainingHours <= 0.0) {
                        detail = "переработка " + formatOneDecimal(std::abs(*r.remainingHours)) + " ч";
                    } else if (r.remainingDays.has_value() && *r.remainingDays <= 0) {
                        detail = "просрочка " + std::to_string(std::abs(*r.remainingDays)) + " дн.";
                    }
                    alerts.push_back({
                        AlertSeverity::Critical,
                        "ТО",
                        "UAV",
                        u.id,
                        "Борт " + u.tailNumber + ": ПРОСРОЧЕНО " + r.ruleTitle +
                            (detail.empty() ? "" : " (" + detail + ")")
                    });
                } else if (r.isWarning) {
                    std::string detail;
                    if (r.remainingHours.has_value()) {
                        detail += "ост. " + formatOneDecimal(*r.remainingHours) + " ч";
                    }
                    if (r.remainingDays.has_value()) {
                        if (!detail.empty()) detail += " / ";
                        detail += std::to_string(*r.remainingDays) + " дн.";
                    }
                    alerts.push_back({
                        AlertSeverity::Warning,
                        "ТО",
                        "UAV",
                        u.id,
                        "Борт " + u.tailNumber + ": скоро " + r.ruleTitle + " (" + detail + ")"
                    });
                }
            }
        }
    }

    // 2. Операторы: истёкшие и приближающиеся сроки допуска (порог 14 дней)
    for (const auto& op : operators) {
        if (!op.active) continue;
        const int daysLeft = op.permitExpiryDayNumber - todayDayNumber;
        if (daysLeft < 0) {
            alerts.push_back({
                AlertSeverity::Critical,
                "Допуск",
                "OPERATOR",
                op.id,
                "Оператор " + op.fullName + ": ДОПУСК ИСТЁК (" + formatIsoToRuDate(op.permitExpiryIso) + ")"
            });
        } else if (daysLeft <= thresholds.permitDaysWarn) {
            alerts.push_back({
                AlertSeverity::Warning,
                "Допуск",
                "OPERATOR",
                op.id,
                "Оператор " + op.fullName + ": допуск истекает через " +
                    std::to_string(daysLeft) + " дн. (" + formatIsoToRuDate(op.permitExpiryIso) + ")"
            });
        }
    }

    // 3. АКБ: выработка ресурса >= 90 % и 100 % (REQ-FUNC-016)
    for (const auto& bat : batteries) {
        if (bat.retired || bat.maxCycles <= 0) continue;
        const double ratio = bat.usageRatio();
        const int pct = static_cast<int>(std::round(ratio * 100.0));
        if (bat.usedCycles >= bat.maxCycles) {
            alerts.push_back({
                AlertSeverity::Critical,
                "АКБ",
                "BATTERY",
                bat.id,
                "АКБ " + bat.inventoryNumber + ": РЕСУРС ВЫРАБОТАН (" +
                    std::to_string(bat.usedCycles) + "/" + std::to_string(bat.maxCycles) + " циклов)"
            });
        } else if (ratio + 1e-9 >= thresholds.batteryWarnRatio) {
            alerts.push_back({
                AlertSeverity::Warning,
                "АКБ",
                "BATTERY",
                bat.id,
                "АКБ " + bat.inventoryNumber + ": выработано " + std::to_string(pct) + " % ресурса (" +
                    std::to_string(bat.usedCycles) + "/" + std::to_string(bat.maxCycles) + ")"
            });
        }
    }

    // Сортировка по критичности: сначала Critical (красные), затем Warning (жёлтые)
    std::stable_sort(alerts.begin(), alerts.end(), [](const AlertItem& a, const AlertItem& b) {
        return static_cast<int>(a.severity) > static_cast<int>(b.severity);
    });
    return alerts;
}

} // namespace uavlog
