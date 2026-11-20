#include "core/AdmissionService.h"
#include "core/UavStateMachine.h"

#include <algorithm>

namespace uavlog {

AdmissionResult AdmissionService::evaluate(const AdmissionContext& ctx) {
    AdmissionResult res;

    // 0. Валидация длительности полёта (0 < duration <= 720 мин)
    if (ctx.durationMinutes <= 0) {
        res.durationOk = false;
        res.reasons.push_back("Время посадки должно быть позже времени взлёта");
    } else if (ctx.durationMinutes > kMaxFlightDurationMinutes) {
        res.durationOk = false;
        res.reasons.push_back("Длительность одного полёта не может превышать 12 часов");
    } else {
        res.durationOk = true;
    }

    // 1. Фактор uavOk: борт в состоянии «Исправен»
    res.uavOk = UavStateMachine::canFly(ctx.uav.status);
    if (!res.uavOk) {
        res.reasons.push_back("Борт " + ctx.uav.tailNumber + " не допущен (статус: «" +
                              toRussianStatus(ctx.uav.status) + "»)");
    }

    // 2. Фактор maintOk: ни один вид ТО не просрочен
    res.maintOk = true;
    for (const auto& rem : ctx.maintRemainders) {
        if (rem.isOverdue) {
            res.maintOk = false;
            res.reasons.push_back("Просрочено ТО «" + rem.ruleTitle + "»");
        }
    }

    // 3. Фактор operatorOk: допуск оператора действует на дату полёта и покрывает тип БЛА
    const bool permitActive = ctx.op.permitExpiryDayNumber >= ctx.flightDayNumber;
    const bool typeAllowed = std::find(ctx.op.allowedUavTypeIds.begin(),
                                       ctx.op.allowedUavTypeIds.end(),
                                       ctx.uav.uavTypeId) != ctx.op.allowedUavTypeIds.end();
    res.operatorOk = permitActive && typeAllowed;
    if (!permitActive) {
        res.reasons.push_back("Срок допуска оператора " + ctx.op.fullName + " истёк (" +
                              ctx.op.permitExpiryIso + ")");
    }
    if (!typeAllowed) {
        res.reasons.push_back("Оператор " + ctx.op.fullName + " не допущен к типу БЛА «" +
                              ctx.uav.typeName + "»");
    }

    // 4. Фактор batteryOk: АКБ не списана и не выработала ресурс (cycles < maxCycles)
    res.batteryOk = !ctx.battery.retired && ctx.battery.usedCycles < ctx.battery.maxCycles;
    if (ctx.battery.retired) {
        res.reasons.push_back("АКБ " + ctx.battery.inventoryNumber + " списана");
    } else if (ctx.battery.usedCycles >= ctx.battery.maxCycles) {
        res.reasons.push_back("АКБ " + ctx.battery.inventoryNumber + " выработала ресурс (" +
                              std::to_string(ctx.battery.usedCycles) + "/" +
                              std::to_string(ctx.battery.maxCycles) + " циклов)");
    }

    // 5. Фактор preflightOk: пройден предполётный осмотр без критичных замечаний (F-15, REQ-FUNC-017)
    res.preflightOk = ctx.preflight.hasInspectionOnDate &&
                      ctx.preflight.passedWithoutCriticalDefects;
    if (!ctx.preflight.hasInspectionOnDate) {
        res.reasons.push_back("На дату полёта не проведён предполётный осмотр по чек-листу (F-15)");
    } else if (!ctx.preflight.passedWithoutCriticalDefects) {
        res.reasons.push_back("По предполётному осмотру выявлены критичные замечания (F-15)");
    }

    res.allowed = res.durationOk && res.uavOk && res.maintOk &&
                  res.operatorOk && res.batteryOk && res.preflightOk;
    return res;
}

} // namespace uavlog
