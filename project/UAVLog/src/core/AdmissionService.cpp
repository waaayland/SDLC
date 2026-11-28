#include "core/AdmissionService.h"
#include "core/UavStateMachine.h"

#include <algorithm>

namespace uavlog {

AdmissionResult AdmissionService::evaluateContext(const AdmissionContext& ctx) const {
    return evaluate(ctx);
}

AdmissionResult AdmissionService::evaluate(const AdmissionContext& ctx) {
    AdmissionResult res;

    // 0. Валидация продолжительности полёта (REQ-FUNC-006-3: 0 < duration <= 720 мин)
    if (ctx.durationMinutes <= 0) {
        res.durationOk = false;
        res.reasons.push_back("Время посадки должно быть строго позже времени взлёта");
    } else if (ctx.durationMinutes > kMaxFlightDurationMinutes) {
        res.durationOk = false;
        res.reasons.push_back("Продолжительность одного полёта не может превышать 12 часов (720 мин)");
    } else {
        res.durationOk = true;
    }

    // 1. Состояние борта (фактор 1: борт в состоянии «Исправен»)
    if (ctx.uav.id <= 0) {
        res.uavOk = false;
        res.reasons.push_back("Не выбран борт БЛА");
    } else if (!UavStateMachine::canFly(ctx.uav.status)) {
        res.uavOk = false;
        res.reasons.push_back("Борт «" + ctx.uav.tailNumber + "» не допущен: состояние «" +
                              toRussianStatus(ctx.uav.status) + "»");
    } else {
        res.uavOk = true;
    }

    // 2. Отсутствие просроченного ТО (фактор 2)
    res.maintOk = true;
    for (const auto& rem : ctx.maintRemainders) {
        if (rem.isOverdue) {
            res.maintOk = false;
            res.reasons.push_back("Просрочен регламент ТО «" + rem.ruleTitle + "» для борта «" +
                                  ctx.uav.tailNumber + "»");
        }
    }

    // 3. Действующий допуск оператора на дату полёта и на тип БЛА (фактор 3)
    res.operatorOk = true;
    if (ctx.op.id <= 0 || !ctx.op.active) {
        res.operatorOk = false;
        res.reasons.push_back("Не выбран активный оператор БЛА");
    } else {
        if (ctx.op.permitExpiryDayNumber < ctx.flightDayNumber) {
            res.operatorOk = false;
            res.reasons.push_back("Истёк срок допуска оператора «" + ctx.op.fullName +
                                  "» (действовал до " + formatIsoToRuDate(ctx.op.permitExpiryIso) + ")");
        }
        const auto& allowedTypes = ctx.op.allowedUavTypeIds;
        if (ctx.uav.uavTypeId > 0 &&
            std::find(allowedTypes.begin(), allowedTypes.end(), ctx.uav.uavTypeId) == allowedTypes.end()) {
            res.operatorOk = false;
            res.reasons.push_back("У оператора «" + ctx.op.fullName +
                                  "» отсутствует допуск на тип БЛА «" + ctx.uav.typeName + "»");
        }
    }

    // 4. Невыработанный ресурс АКБ (фактор 4: used_cycles < max_cycles, REQ-FUNC-016)
    if (ctx.battery.id <= 0) {
        res.batteryOk = false;
        res.reasons.push_back("Не выбрана аккумуляторная батарея (АКБ)");
    } else if (ctx.battery.retired) {
        res.batteryOk = false;
        res.reasons.push_back("АКБ «" + ctx.battery.inventoryNumber + "» списана");
    } else if (ctx.battery.isExhausted()) {
        res.batteryOk = false;
        res.reasons.push_back("АКБ «" + ctx.battery.inventoryNumber + "» выработала 100 % ресурса (" +
                              std::to_string(ctx.battery.usedCycles) + "/" +
                              std::to_string(ctx.battery.maxCycles) + " циклов)");
    } else {
        res.batteryOk = true;
    }

    // 5. Предполётный осмотр по чек-листу на дату полёта (фактор 5, модуль F-15, REQ-FUNC-017)
    if (!ctx.preflight.hasInspectionOnDate) {
        res.preflightOk = false;
        res.reasons.push_back("Не проведён предполётный осмотр борта «" + ctx.uav.tailNumber +
                              "» на дату полёта (модуль F-15)");
    } else if (!ctx.preflight.passedWithoutCriticalDefects) {
        res.preflightOk = false;
        res.reasons.push_back("Предполётный осмотр борта «" + ctx.uav.tailNumber +
                              "» выявил критичные замечания (вылет заблокирован)");
    } else {
        res.preflightOk = true;
    }

    res.allowed = res.durationOk && res.uavOk && res.maintOk &&
                  res.operatorOk && res.batteryOk && res.preflightOk;
    return res;
}

} // namespace uavlog
