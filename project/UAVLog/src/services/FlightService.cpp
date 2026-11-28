#include "services/FlightService.h"
#include "core/AdmissionService.h"
#include "core/MaintenanceEngine.h"
#include "core/RbacGuard.h"
#include "core/UavStateMachine.h"

#include <QSqlError>
#include <QSqlQuery>

namespace uavlog {

namespace {
int isoToJulian(const QString& isoDate) {
    const QDate d = QDate::fromString(isoDate, Qt::ISODate);
    return d.isValid() ? static_cast<int>(d.toJulianDay()) : 0;
}
} // namespace

FlightService::FlightService(DatabaseManager& db, AuditLogger& audit, FleetService& fleetService)
    : m_db(db), m_audit(audit), m_fleet(fleetService) {}

PreflightSummary FlightService::loadPreflightSummary(int uavId, const QDate& date) {
    PreflightSummary ps;
    QSqlQuery q(m_db.database());
    q.prepare(R"(
        SELECT id, passed, check_dt FROM preflight_checks
        WHERE uav_id = :uid AND check_dt = :dt
        ORDER BY id DESC LIMIT 1
    )");
    q.bindValue(":uid", uavId);
    q.bindValue(":dt", date.toString(Qt::ISODate));
    if (q.exec() && q.next()) {
        ps.hasInspectionOnDate = true;
        ps.checkId = q.value(0).toInt();
        ps.passedWithoutCriticalDefects = q.value(1).toInt() != 0;
        ps.inspectionDateIso = q.value(2).toString().toStdString();
    }
    return ps;
}

AdmissionResult FlightService::evaluateAdmission(const FlightInputDto& dto) {
    const QDate refDate = dto.flightDate.isValid() ? dto.flightDate : QDate::currentDate();
    const auto dash = m_fleet.loadDashboard(refDate);
    const auto ops = m_fleet.loadOperators(false);
    const auto bats = m_fleet.loadBatteries(false);

    AdmissionContext ctx;
    ctx.flightDayNumber = static_cast<int>(refDate.toJulianDay());
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
    ctx.preflight = loadPreflightSummary(dto.uavId, refDate);

    return AdmissionService::evaluate(ctx);
}

bool FlightService::recalculateUavAndBatteryTx(int uavId, int batteryId, const QDate& refDate, QString* errorOut) {
    // Детерминированный пересчёт агрегатов из таблицы flights (ADR-0004, REQ-FUNC-007)
    if (uavId > 0) {
        QSqlQuery updUav(m_db.database());
        updUav.prepare(R"(
            UPDATE uavs SET
                total_flight_min = COALESCE((SELECT SUM(duration_min) FROM flights WHERE uav_id = :uid1), 0),
                total_flights    = (SELECT COUNT(*) FROM flights WHERE uav_id = :uid2),
                last_flight_date = (SELECT MAX(flight_date) FROM flights WHERE uav_id = :uid3)
            WHERE id = :uid4
        )");
        updUav.bindValue(":uid1", uavId);
        updUav.bindValue(":uid2", uavId);
        updUav.bindValue(":uid3", uavId);
        updUav.bindValue(":uid4", uavId);
        if (!updUav.exec()) {
            if (errorOut) *errorOut = updUav.lastError().text();
            return false;
        }

        // Проверяем остатки до ТО и при необходимости переводим Serviceable -> DueMaintenance
        QSqlQuery uq(m_db.database());
        uq.prepare("SELECT uav_type_id, state, total_flight_min, commissioned_date FROM uavs WHERE id = :id;");
        uq.bindValue(":id", uavId);
        if (uq.exec() && uq.next()) {
            Uav u;
            u.id = uavId;
            u.uavTypeId = uq.value(0).toInt();
            u.status = fromDbStatus(uq.value(1).toString().toStdString());
            u.totalFlightMin = uq.value(2).toInt();
            const int commDay = isoToJulian(uq.value(3).toString());

            const auto rules = m_fleet.loadMaintenanceRules(u.uavTypeId);
            const auto recs = m_fleet.loadLastMaintRecords(uavId);
            const auto thresholds = m_fleet.loadAlertThresholds();
            const auto rems = MaintenanceEngine::calculateRemainders(
                u, commDay, static_cast<int>(refDate.toJulianDay()), rules, recs, thresholds);

            bool anyOverdue = false;
            for (const auto& r : rems) {
                if (r.isOverdue) { anyOverdue = true; break; }
            }

            if (anyOverdue && u.status == UavStatus::Serviceable) {
                if (auto next = UavStateMachine::transition(u.status, UavEvent::MaintenanceThresholdReached)) {
                    QSqlQuery st(m_db.database());
                    st.prepare("UPDATE uavs SET state = :st WHERE id = :id;");
                    st.bindValue(":st", QString::fromStdString(toDbStatus(*next)));
                    st.bindValue(":id", uavId);
                    if (!st.exec()) {
                        if (errorOut) *errorOut = st.lastError().text();
                        return false;
                    }
                }
            } else if (!anyOverdue && u.status == UavStatus::DueMaintenance) {
                // Например, после удаления ошибочного полёта наработка вернулась ниже порога ТО
                QSqlQuery st(m_db.database());
                st.prepare("UPDATE uavs SET state = 'Serviceable' WHERE id = :id;");
                st.bindValue(":id", uavId);
                st.exec();
            }
        }
    }

    if (batteryId > 0) {
        QSqlQuery updBat(m_db.database());
        updBat.prepare("UPDATE batteries SET used_cycles = (SELECT COUNT(*) FROM flights WHERE battery_id = :bid1) WHERE id = :bid2;");
        updBat.bindValue(":bid1", batteryId);
        updBat.bindValue(":bid2", batteryId);
        if (!updBat.exec()) {
            if (errorOut) *errorOut = updBat.lastError().text();
            return false;
        }
    }

    return true;
}

bool FlightService::registerFlight(const FlightInputDto& dto,
                                   const UserAccount& actor,
                                   int* flightIdOut,
                                   QString* errorOut) {
    // 1. Проверка RBAC (REQ-SEC-002)
    try {
        RbacGuard::requirePermission(actor.role, Permission::RegisterFlight);
    } catch (const std::exception& ex) {
        if (errorOut) *errorOut = QString::fromUtf8(ex.what());
        m_audit.logError("WARN", "FlightService::registerFlight", QString::fromUtf8(ex.what()));
        return false;
    }

    // 2. Повторная серверная проверка 5-факторного допуска (REQ-FUNC-006-3, UC-05a)
    const auto admission = evaluateAdmission(dto);
    if (!admission.allowed) {
        QStringList reasons;
        for (const auto& r : admission.reasons) reasons << QString::fromStdString(r);
        if (errorOut) *errorOut = reasons.join("; ");
        return false;
    }

    const auto preflight = loadPreflightSummary(dto.uavId, dto.flightDate);

    // 3. Атомарная транзакция BEGIN IMMEDIATE (ADR-0002, REQ-REL-001)
    if (!m_db.beginImmediateQ(errorOut)) {
        return false;
    }

    QSqlQuery ins(m_db.database());
    ins.prepare(R"(
        INSERT INTO flights (uav_id, operator_id, battery_id, preflight_id, flight_date,
                             takeoff, landing, duration_min, task_and_area, notes, created_by)
        VALUES (:uid, :oid, :bid, :pfid, :dt, :t1, :t2, :dur, :task, :notes, :cb)
    )");
    ins.bindValue(":uid", dto.uavId);
    ins.bindValue(":oid", dto.operatorId);
    ins.bindValue(":bid", dto.batteryId);
    ins.bindValue(":pfid", preflight.checkId.has_value() ? QVariant(*preflight.checkId) : QVariant());
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
        m_audit.logError("ERROR", "FlightService::registerFlight", ins.lastError().text());
        return false;
    }
    const int flightId = ins.lastInsertId().toInt();
    if (flightIdOut) *flightIdOut = flightId;

    // 4. Детерминированный пересчёт налёта, циклов АКБ и сроков ТО (ADR-0004)
    if (!recalculateUavAndBatteryTx(dto.uavId, dto.batteryId, dto.flightDate, errorOut)) {
        m_db.rollback();
        m_audit.logError("ERROR", "FlightService::registerFlight", errorOut ? *errorOut : "recalc error");
        return false;
    }

    // 5. Сквозной аудит в той же транзакции (REQ-SEC-004, UC-11)
    if (!AuditLogger::writeAuditRecord(
            m_db.database(), actor.id, "FLIGHT_CREATE", "flights", flightId, "",
            QString("uav=%1;op=%2;bat=%3;date=%4;min=%5")
                .arg(dto.uavId).arg(dto.operatorId).arg(dto.batteryId)
                .arg(dto.flightDate.toString(Qt::ISODate)).arg(dto.durationMinutes),
            errorOut)) {
        m_db.rollback();
        return false;
    }

    if (!m_db.commitQ(errorOut)) {
        m_db.rollback();
        return false;
    }
    return true;
}

bool FlightService::updateFlight(int flightId,
                                 const FlightInputDto& dto,
                                 const UserAccount& actor,
                                 QString* errorOut) {
    try {
        RbacGuard::requirePermission(actor.role, Permission::EditOrDeleteFlight);
    } catch (const std::exception& ex) {
        if (errorOut) *errorOut = QString::fromUtf8(ex.what());
        return false;
    }

    if (dto.durationMinutes <= 0 || dto.durationMinutes > AdmissionService::kMaxFlightDurationMinutes) {
        if (errorOut) *errorOut = "Длительность полёта должна быть от 1 мин до 12 ч (720 мин)";
        return false;
    }

    QSqlQuery oldQ(m_db.database());
    oldQ.prepare("SELECT uav_id, battery_id, duration_min, flight_date FROM flights WHERE id = :id;");
    oldQ.bindValue(":id", flightId);
    if (!oldQ.exec() || !oldQ.next()) {
        if (errorOut) *errorOut = "Запись полёта не найдена";
        return false;
    }
    const int oldUavId = oldQ.value(0).toInt();
    const int oldBatId = oldQ.value(1).toInt();
    const int oldMin = oldQ.value(2).toInt();
    const QString oldDate = oldQ.value(3).toString();

    if (!m_db.beginImmediateQ(errorOut)) return false;

    QSqlQuery upd(m_db.database());
    upd.prepare(R"(
        UPDATE flights SET uav_id=:uid, operator_id=:oid, battery_id=:bid, flight_date=:dt,
                           takeoff=:t1, landing=:t2, duration_min=:dur, task_and_area=:task, notes=:nt
        WHERE id=:id
    )");
    upd.bindValue(":uid", dto.uavId);
    upd.bindValue(":oid", dto.operatorId);
    upd.bindValue(":bid", dto.batteryId);
    upd.bindValue(":dt", dto.flightDate.toString(Qt::ISODate));
    upd.bindValue(":t1", dto.takeoffTime);
    upd.bindValue(":t2", dto.landingTime);
    upd.bindValue(":dur", dto.durationMinutes);
    upd.bindValue(":task", dto.taskAndArea);
    upd.bindValue(":nt", dto.notes);
    upd.bindValue(":id", flightId);

    if (!upd.exec()) {
        if (errorOut) *errorOut = upd.lastError().text();
        m_db.rollback();
        return false;
    }

    // Детерминированный пересчёт как старого, так и нового борта/АКБ (ADR-0004)
    const QDate refDate = QDate::currentDate();
    if (!recalculateUavAndBatteryTx(dto.uavId, dto.batteryId, refDate, errorOut)) {
        m_db.rollback();
        return false;
    }
    if (oldUavId != dto.uavId || oldBatId != dto.batteryId) {
        if (!recalculateUavAndBatteryTx(oldUavId != dto.uavId ? oldUavId : 0,
                                        oldBatId != dto.batteryId ? oldBatId : 0,
                                        refDate, errorOut)) {
            m_db.rollback();
            return false;
        }
    }

    AuditLogger::writeAuditRecord(
        m_db.database(), actor.id, "FLIGHT_UPDATE", "flights", flightId,
        QString("uav=%1;bat=%2;date=%3;min=%4").arg(oldUavId).arg(oldBatId).arg(oldDate).arg(oldMin),
        QString("uav=%1;bat=%2;date=%3;min=%4").arg(dto.uavId).arg(dto.batteryId)
            .arg(dto.flightDate.toString(Qt::ISODate)).arg(dto.durationMinutes));

    return m_db.commitQ(errorOut);
}

bool FlightService::deleteFlight(int flightId, const UserAccount& actor, QString* errorOut) {
    try {
        RbacGuard::requirePermission(actor.role, Permission::EditOrDeleteFlight);
    } catch (const std::exception& ex) {
        if (errorOut) *errorOut = QString::fromUtf8(ex.what());
        return false;
    }

    QSqlQuery oldQ(m_db.database());
    oldQ.prepare("SELECT uav_id, battery_id, duration_min, flight_date FROM flights WHERE id = :id;");
    oldQ.bindValue(":id", flightId);
    if (!oldQ.exec() || !oldQ.next()) {
        if (errorOut) *errorOut = "Запись полёта не найдена";
        return false;
    }
    const int oldUavId = oldQ.value(0).toInt();
    const int oldBatId = oldQ.value(1).toInt();
    const int oldMin = oldQ.value(2).toInt();
    const QString oldDate = oldQ.value(3).toString();

    if (!m_db.beginImmediateQ(errorOut)) return false;

    QSqlQuery del(m_db.database());
    del.prepare("DELETE FROM flights WHERE id = :id;");
    del.bindValue(":id", flightId);
    if (!del.exec()) {
        if (errorOut) *errorOut = del.lastError().text();
        m_db.rollback();
        return false;
    }

    if (!recalculateUavAndBatteryTx(oldUavId, oldBatId, QDate::currentDate(), errorOut)) {
        m_db.rollback();
        return false;
    }

    AuditLogger::writeAuditRecord(
        m_db.database(), actor.id, "FLIGHT_DELETE", "flights", flightId,
        QString("uav=%1;bat=%2;date=%3;min=%4").arg(oldUavId).arg(oldBatId).arg(oldDate).arg(oldMin),
        "deleted");

    return m_db.commitQ(errorOut);
}

std::vector<FlightRecord> FlightService::loadFlights(int uavId,
                                                     int operatorId,
                                                     const QDate& fromDate,
                                                     const QDate& toDate) {
    std::vector<FlightRecord> list;
    QString sql = R"(
        SELECT fl.id, fl.uav_id, u.tail_number, fl.operator_id, op.full_name,
               fl.battery_id, b.inventory_no, fl.preflight_id, fl.flight_date,
               fl.takeoff, fl.landing, fl.duration_min, COALESCE(fl.task_and_area,''),
               COALESCE(fl.notes,''), COALESCE(fl.created_by, 0)
        FROM flights fl
        JOIN uavs u ON u.id = fl.uav_id
        JOIN operators op ON op.id = fl.operator_id
        JOIN batteries b ON b.id = fl.battery_id
        WHERE 1=1
    )";
    if (uavId > 0) sql += " AND fl.uav_id = :uid";
    if (operatorId > 0) sql += " AND fl.operator_id = :oid";
    if (fromDate.isValid()) sql += " AND fl.flight_date >= :dfrom";
    if (toDate.isValid()) sql += " AND fl.flight_date <= :dto";
    sql += " ORDER BY fl.flight_date DESC, fl.id DESC;";

    QSqlQuery q(m_db.database());
    q.prepare(sql);
    if (uavId > 0) q.bindValue(":uid", uavId);
    if (operatorId > 0) q.bindValue(":oid", operatorId);
    if (fromDate.isValid()) q.bindValue(":dfrom", fromDate.toString(Qt::ISODate));
    if (toDate.isValid()) q.bindValue(":dto", toDate.toString(Qt::ISODate));
    q.exec();

    while (q.next()) {
        FlightRecord r;
        r.id = q.value(0).toInt();
        r.uavId = q.value(1).toInt();
        r.uavTailNumber = q.value(2).toString().toStdString();
        r.operatorId = q.value(3).toInt();
        r.operatorName = q.value(4).toString().toStdString();
        r.batteryId = q.value(5).toInt();
        r.batteryInventory = q.value(6).toString().toStdString();
        if (!q.value(7).isNull()) r.preflightId = q.value(7).toInt();
        r.flightDateIso = q.value(8).toString().toStdString();
        r.takeoffTime = q.value(9).toString().toStdString();
        r.landingTime = q.value(10).toString().toStdString();
        r.durationMinutes = q.value(11).toInt();
        r.taskAndArea = q.value(12).toString().toStdString();
        r.notes = q.value(13).toString().toStdString();
        r.createdByUserId = q.value(14).toInt();
        list.push_back(std::move(r));
    }
    return list;
}

} // namespace uavlog
