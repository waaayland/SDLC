#include "services/FleetService.h"
#include "core/AdmissionService.h"
#include "core/MaintenanceEngine.h"
#include "core/RbacGuard.h"
#include "core/UavStateMachine.h"

#include <QSqlError>
#include <QSqlQuery>

namespace uavlog {

namespace {
int toDayNumber(const QString& isoDate) {
    const QDate d = QDate::fromString(isoDate, Qt::ISODate);
    return d.isValid() ? static_cast<int>(d.toJulianDay()) : 0;
}
} // namespace

FleetService::FleetService(DatabaseManager& db, AuditLogger& audit)
    : m_db(db), m_audit(audit) {}

std::vector<MaintenanceRule> FleetService::loadMaintenanceRules() {
    std::vector<MaintenanceRule> rules;
    QSqlQuery q(m_db.database());
    q.exec("SELECT id, uav_type_id, title, interval_hours, interval_days, works_checklist FROM maintenance_rules ORDER BY id;");
    while (q.next()) {
        MaintenanceRule r;
        r.id = q.value(0).toInt();
        r.uavTypeId = q.value(1).toInt();
        r.title = q.value(2).toString().toStdString();
        if (!q.value(3).isNull()) r.intervalHours = q.value(3).toDouble();
        if (!q.value(4).isNull()) r.intervalDays = q.value(4).toInt();
        r.worksChecklist = q.value(5).toString().toStdString();
        rules.push_back(r);
    }
    return rules;
}

std::vector<MaintenanceRecordSummary> FleetService::loadLastMaintRecords(int uavId) {
    std::vector<MaintenanceRecordSummary> recs;
    QSqlQuery q(m_db.database());
    q.prepare("SELECT rule_id, flight_min_at_maint, performed_date FROM maintenance_records WHERE uav_id = :id;");
    q.bindValue(":id", uavId);
    q.exec();
    while (q.next()) {
        MaintenanceRecordSummary s;
        s.ruleId = q.value(0).toInt();
        s.flightMinAtMaint = q.value(1).toInt();
        s.performedDayNumber = toDayNumber(q.value(2).toString());
        recs.push_back(s);
    }
    return recs;
}

PreflightSummary FleetService::loadPreflightSummary(int uavId, const QDate& date) {
    PreflightSummary ps;
    QSqlQuery q(m_db.database());
    q.prepare(R"(
        SELECT passed, check_date FROM preflight_checks
        WHERE uav_id = :uid AND check_date = :dt
        ORDER BY id DESC LIMIT 1
    )");
    q.bindValue(":uid", uavId);
    q.bindValue(":dt", date.toString(Qt::ISODate));
    if (q.exec() && q.next()) {
        ps.hasInspectionOnDate = true;
        ps.passedWithoutCriticalDefects = q.value(0).toInt() != 0;
        ps.inspectionDateIso = q.value(1).toString().toStdString();
    }
    return ps;
}

int FleetService::loadUavCommissionDay(int uavId) {
    QSqlQuery q(m_db.database());
    q.prepare("SELECT commission_date FROM uavs WHERE id = :id;");
    q.bindValue(":id", uavId);
    if (q.exec() && q.next()) {
        return toDayNumber(q.value(0).toString());
    }
    return static_cast<int>(QDate::currentDate().toJulianDay());
}

std::vector<OperatorInfo> FleetService::loadOperators() {
    std::vector<OperatorInfo> ops;
    QSqlQuery q(m_db.database());
    q.exec("SELECT id, full_name, position, permit_number, permit_expiry_date FROM operators ORDER BY full_name;");
    while (q.next()) {
        OperatorInfo op;
        op.id = q.value(0).toInt();
        op.fullName = q.value(1).toString().toStdString();
        op.position = q.value(2).toString().toStdString();
        op.permitNumber = q.value(3).toString().toStdString();
        const QString expIso = q.value(4).toString();
        op.permitExpiryIso = QDate::fromString(expIso, Qt::ISODate).toString("dd.MM.yyyy").toStdString();
        op.permitExpiryDayNumber = toDayNumber(expIso);

        QSqlQuery qt(m_db.database());
        qt.prepare("SELECT uav_type_id FROM operator_uav_types WHERE operator_id = :id;");
        qt.bindValue(":id", op.id);
        qt.exec();
        while (qt.next()) {
            op.allowedUavTypeIds.push_back(qt.value(0).toInt());
        }
        ops.push_back(op);
    }
    return ops;
}

std::vector<Battery> FleetService::loadBatteries(bool onlyUsable) {
    std::vector<Battery> list;
    QSqlQuery q(m_db.database());
    const QString sql = onlyUsable
        ? "SELECT id, inventory_number, type_name, max_cycles, used_cycles, assigned_uav_id, retired FROM batteries WHERE retired = 0 ORDER BY inventory_number;"
        : "SELECT id, inventory_number, type_name, max_cycles, used_cycles, assigned_uav_id, retired FROM batteries ORDER BY inventory_number;";
    q.exec(sql);
    while (q.next()) {
        Battery b;
        b.id = q.value(0).toInt();
        b.inventoryNumber = q.value(1).toString().toStdString();
        b.typeName = q.value(2).toString().toStdString();
        b.maxCycles = q.value(3).toInt();
        b.usedCycles = q.value(4).toInt();
        if (!q.value(5).isNull()) b.assignedUavId = q.value(5).toInt();
        b.retired = q.value(6).toInt() != 0;
        list.push_back(b);
    }
    return list;
}

FleetDashboardData FleetService::loadDashboard(const QDate& today) {
    FleetDashboardData data;
    const auto rules = loadMaintenanceRules();
    const int todayNum = static_cast<int>(today.toJulianDay());

    QSqlQuery q(m_db.database());
    q.exec(R"(
        SELECT u.id, u.tail_number, u.uav_type_id, t.name, u.serial_number,
               u.commission_date, u.standard_payload, u.status,
               u.total_flight_min, u.total_flights, u.last_flight_date
        FROM uavs u
        JOIN uav_types t ON t.id = u.uav_type_id
        ORDER BY u.tail_number
    )");
    while (q.next()) {
        Uav u;
        u.id = q.value(0).toInt();
        u.tailNumber = q.value(1).toString().toStdString();
        u.uavTypeId = q.value(2).toInt();
        u.typeName = q.value(3).toString().toStdString();
        u.serialNumber = q.value(4).toString().toStdString();
        u.commissionDateIso = q.value(5).toString().toStdString();
        u.standardPayload = q.value(6).toString().toStdString();
        u.status = fromDbStatus(q.value(7).toString().toStdString());
        u.totalFlightMin = q.value(8).toInt();
        u.totalFlights = q.value(9).toInt();
        u.lastFlightDateIso = q.value(10).toString().toStdString();

        const auto recs = loadLastMaintRecords(u.id);
        auto rem = MaintenanceEngine::calculateRemainders(
            u, toDayNumber(QString::fromStdString(u.commissionDateIso)), todayNum, rules, recs);

        data.uavs.push_back(u);
        data.remainders.push_back(std::move(rem));
    }

    const auto ops = loadOperators();
    const auto bats = loadBatteries(false);
    data.alerts = MaintenanceEngine::buildAlerts(data.uavs, data.remainders, ops, bats, todayNum);
    return data;
}

AdmissionResult FleetService::evaluateFlightAdmission(const FlightInputDto& dto) {
    const auto dash = loadDashboard(dto.flightDate);
    const auto ops = loadOperators();
    const auto bats = loadBatteries(false);

    AdmissionContext ctx;
    ctx.flightDayNumber = static_cast<int>(dto.flightDate.toJulianDay());
    ctx.durationMinutes = dto.durationMinutes;

    for (std::size_t i = 0; i < dash.uavs.size(); ++i) {
        if (dash.uavs[i].id == dto.uavId) {
            ctx.uav = dash.uavs[i];
            ctx.maintRemainders = dash.remainders[i];
            break;
        }
    }
    for (const auto& op : ops) {
        if (op.id == dto.operatorId) {
            ctx.op = op;
            break;
        }
    }
    for (const auto& bat : bats) {
        if (bat.id == dto.batteryId) {
            ctx.battery = bat;
            break;
        }
    }
    ctx.preflight = loadPreflightSummary(dto.uavId, dto.flightDate);

    return AdmissionService::evaluate(ctx);
}

bool FleetService::registerFlight(const FlightInputDto& dto,
                                  const UserAccount& actor,
                                  QString* errorOut) {
    // 1. Проверка RBAC на уровне сервисов (REQ-SEC-002)
    try {
        RbacGuard::requirePermission(actor.role, Permission::RegisterFlight);
    } catch (const std::exception& ex) {
        if (errorOut) *errorOut = QString::fromUtf8(ex.what());
        m_audit.logError("WARN", "FleetService::registerFlight", QString::fromUtf8(ex.what()));
        return false;
    }

    // 2. Повторная серверная проверка 5-факторного допуска (UC-05a)
    const auto admission = evaluateFlightAdmission(dto);
    if (!admission.allowed) {
        QStringList reasons;
        for (const auto& r : admission.reasons) reasons << QString::fromStdString(r);
        if (errorOut) *errorOut = reasons.join("; ");
        return false;
    }

    // 3. Атомарная транзакция BEGIN IMMEDIATE (ADR-0002, REQ-REL-001, UML 04-sequence)
    if (!m_db.beginImmediate(errorOut)) {
        return false;
    }

    QSqlQuery ins(m_db.database());
    ins.prepare(R"(
        INSERT INTO flights (uav_id, operator_id, battery_id, flight_date,
                             takeoff_time, landing_time, duration_min, task_and_area, notes, created_by)
        VALUES (:uid, :oid, :bid, :dt, :t1, :t2, :dur, :task, :notes, :cb)
    )");
    ins.bindValue(":uid", dto.uavId);
    ins.bindValue(":oid", dto.operatorId);
    ins.bindValue(":bid", dto.batteryId);
    ins.bindValue(":dt", dto.flightDate.toString(Qt::ISODate));
    ins.bindValue(":t1", dto.takeoffTime);
    ins.bindValue(":t2", dto.landingTime);
    ins.bindValue(":dur", dto.durationMinutes);
    ins.bindValue(":task", dto.taskAndArea);
    ins.bindValue(":notes", dto.notes);
    ins.bindValue(":cb", actor.id);

    if (!ins.exec()) {
        if (errorOut) *errorOut = ins.lastError().text();
        m_db.rollback();
        m_audit.logError("ERROR", "FleetService::registerFlight", ins.lastError().text());
        return false;
    }
    const int flightId = ins.lastInsertId().toInt();

    // 4. Пересчёт налёта борта и циклов АКБ (UC-05b, ADR-0004)
    QSqlQuery updUav(m_db.database());
    updUav.prepare(R"(
        UPDATE uavs
        SET total_flight_min = total_flight_min + :dur,
            total_flights    = total_flights + 1,
            last_flight_date = :dt
        WHERE id = :uid
    )");
    updUav.bindValue(":dur", dto.durationMinutes);
    updUav.bindValue(":dt", dto.flightDate.toString(Qt::ISODate));
    updUav.bindValue(":uid", dto.uavId);
    if (!updUav.exec()) {
        if (errorOut) *errorOut = updUav.lastError().text();
        m_db.rollback();
        return false;
    }

    QSqlQuery updBat(m_db.database());
    updBat.prepare("UPDATE batteries SET used_cycles = used_cycles + 1 WHERE id = :bid;");
    updBat.bindValue(":bid", dto.batteryId);
    if (!updBat.exec()) {
        if (errorOut) *errorOut = updBat.lastError().text();
        m_db.rollback();
        return false;
    }

    // Проверка достижения порога ТО -> переход Serviceable -> DueMaintenance
    QSqlQuery uq(m_db.database());
    uq.prepare("SELECT uav_type_id, status, total_flight_min, commission_date FROM uavs WHERE id = :id;");
    uq.bindValue(":id", dto.uavId);
    if (uq.exec() && uq.next()) {
        Uav updated;
        updated.id = dto.uavId;
        updated.uavTypeId = uq.value(0).toInt();
        updated.status = fromDbStatus(uq.value(1).toString().toStdString());
        updated.totalFlightMin = uq.value(2).toInt();
        const int commDay = toDayNumber(uq.value(3).toString());

        const auto remainders = MaintenanceEngine::calculateRemainders(
            updated, commDay, static_cast<int>(dto.flightDate.toJulianDay()),
            loadMaintenanceRules(), loadLastMaintRecords(dto.uavId));
        bool anyOverdue = false;
        for (const auto& r : remainders) {
            if (r.isOverdue) { anyOverdue = true; break; }
        }
        if (anyOverdue) {
            if (auto nextState = UavStateMachine::transition(updated.status, UavEvent::MaintenanceThresholdReached)) {
                QSqlQuery st(m_db.database());
                st.prepare("UPDATE uavs SET status = :st WHERE id = :id;");
                st.bindValue(":st", QString::fromStdString(toDbStatus(*nextState)));
                st.bindValue(":id", dto.uavId);
                st.exec();
            }
        }
    }

    // 5. Запись в журнал аудита внутри той же транзакции (UC-11)
    if (!AuditLogger::writeAuditRecord(
            m_db.database(), actor.id, "FLIGHT_CREATE", "flights", flightId, "",
            QString("uav=%1;op=%2;bat=%3;min=%4").arg(dto.uavId).arg(dto.operatorId).arg(dto.batteryId).arg(dto.durationMinutes),
            errorOut)) {
        m_db.rollback();
        return false;
    }

    if (!m_db.commit(errorOut)) {
        m_db.rollback();
        return false;
    }
    return true;
}

} // namespace uavlog
