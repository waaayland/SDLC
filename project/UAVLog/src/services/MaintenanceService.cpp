#include "services/MaintenanceService.h"
#include "core/MaintenanceEngine.h"
#include "core/RbacGuard.h"
#include "core/UavStateMachine.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlError>
#include <QSqlQuery>

namespace uavlog {

namespace {
int isoToJulian(const QString& isoDate) {
    const QDate d = QDate::fromString(isoDate, Qt::ISODate);
    return d.isValid() ? static_cast<int>(d.toJulianDay()) : 0;
}
} // namespace

MaintenanceService::MaintenanceService(DatabaseManager& db, AuditLogger& audit, FleetService& fleet)
    : m_db(db), m_audit(audit), m_fleet(fleet) {}

bool MaintenanceService::hasOpenCriticalFaults(int uavId) {
    QSqlQuery q(m_db.database());
    q.prepare("SELECT COUNT(*) FROM faults WHERE uav_id = :id AND is_critical = 1 AND resolved_date IS NULL;");
    q.bindValue(":id", uavId);
    return q.exec() && q.next() && q.value(0).toInt() > 0;
}

bool MaintenanceService::hasOverdueMaintenance(int uavId, const QDate& refDate) {
    QSqlQuery uq(m_db.database());
    uq.prepare("SELECT uav_type_id, total_flight_min, commissioned_date FROM uavs WHERE id = :id;");
    uq.bindValue(":id", uavId);
    if (!uq.exec() || !uq.next()) return false;

    Uav u;
    u.id = uavId;
    u.uavTypeId = uq.value(0).toInt();
    u.totalFlightMin = uq.value(1).toInt();
    const int commDay = isoToJulian(uq.value(2).toString());

    const auto rules = m_fleet.loadMaintenanceRules(u.uavTypeId);
    const auto recs = m_fleet.loadLastMaintRecords(uavId);
    const auto th = m_fleet.loadAlertThresholds();
    const auto rems = MaintenanceEngine::calculateRemainders(
        u, commDay, static_cast<int>(refDate.toJulianDay()), rules, recs, th);

    for (const auto& r : rems) {
        if (r.isOverdue) return true;
    }
    return false;
}

bool MaintenanceService::startMaintenance(int uavId, const UserAccount& actor, QString* errorOut) {
    try {
        RbacGuard::requirePermission(actor.role, Permission::RegisterMaintenance);
    } catch (const std::exception& ex) {
        if (errorOut) *errorOut = QString::fromUtf8(ex.what());
        return false;
    }

    QSqlQuery cur(m_db.database());
    cur.prepare("SELECT state FROM uavs WHERE id = :id;");
    cur.bindValue(":id", uavId);
    if (!cur.exec() || !cur.next()) {
        if (errorOut) *errorOut = "Борт не найден";
        return false;
    }
    const UavStatus st = fromDbStatus(cur.value(0).toString().toStdString());
    const auto next = UavStateMachine::transition(st, UavEvent::StartMaintenance);
    if (!next.has_value()) {
        if (errorOut) *errorOut = "Перевод на ТО невозможен из состояния «" +
                                  QString::fromStdString(toRussianStatus(st)) + "»";
        return false;
    }

    if (!m_db.beginImmediateQ(errorOut)) return false;
    QSqlQuery upd(m_db.database());
    upd.prepare("UPDATE uavs SET state = :st WHERE id = :id;");
    upd.bindValue(":st", QString::fromStdString(toDbStatus(*next)));
    upd.bindValue(":id", uavId);
    if (!upd.exec()) {
        if (errorOut) *errorOut = upd.lastError().text();
        m_db.rollback();
        return false;
    }

    AuditLogger::writeAuditRecord(m_db.database(), actor.id, "MAINT_START", "uavs", uavId,
                                  QString::fromStdString(toDbStatus(st)), "InMaintenance");
    return m_db.commitQ(errorOut);
}

bool MaintenanceService::registerMaintenance(int uavId,
                                             int ruleId,
                                             const QDate& performedDate,
                                             const QString& performerName,
                                             const std::vector<MaintenanceWorkCheck>& works,
                                             const QString& notes,
                                             const UserAccount& actor,
                                             int* recordIdOut,
                                             QString* errorOut) {
    try {
        RbacGuard::requirePermission(actor.role, Permission::RegisterMaintenance);
    } catch (const std::exception& ex) {
        if (errorOut) *errorOut = QString::fromUtf8(ex.what());
        return false;
    }

    if (uavId <= 0 || ruleId <= 0 || !performedDate.isValid() || performerName.trimmed().isEmpty()) {
        if (errorOut) *errorOut = "Укажите борт, вид ТО, дату выполнения и исполнителя";
        return false;
    }

    // REQ-FUNC-010: ТО не сохраняется, пока не отмечены все работы или не указана причина невыполнения
    QJsonArray arr;
    for (const auto& w : works) {
        if (!w.completed && w.note.trimmed().isEmpty()) {
            if (errorOut) {
                *errorOut = "Для невыполненной работы «" + w.taskText +
                            "» обязательно укажите причину в столбце «Примечание» (REQ-FUNC-010)";
            }
            return false;
        }
        QJsonObject obj;
        obj["task"] = w.taskText;
        obj["done"] = w.completed;
        obj["note"] = w.note.trimmed();
        arr.append(obj);
    }
    const QString resultsJson = QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));

    QSqlQuery uq(m_db.database());
    uq.prepare("SELECT state, total_flight_min FROM uavs WHERE id = :id;");
    uq.bindValue(":id", uavId);
    if (!uq.exec() || !uq.next()) {
        if (errorOut) *errorOut = "Борт не найден";
        return false;
    }
    const UavStatus curStatus = fromDbStatus(uq.value(0).toString().toStdString());
    const int flightMinAtMaint = uq.value(1).toInt();
    if (UavStateMachine::isTerminal(curStatus)) {
        if (errorOut) *errorOut = "Регистрация ТО для списанного борта запрещена";
        return false;
    }

    if (!m_db.beginImmediateQ(errorOut)) return false;

    QSqlQuery ins(m_db.database());
    ins.prepare(R"(
        INSERT INTO maintenance_records
            (uav_id, rule_id, performed_by, performer_name, performed_date,
             flight_min_at_maint, checklist_results_json, notes)
        VALUES (:uid, :rid, :pby, :pnm, :pdt, :fmin, :cj, :nt)
    )");
    ins.bindValue(":uid", uavId);
    ins.bindValue(":rid", ruleId);
    ins.bindValue(":pby", actor.id);
    ins.bindValue(":pnm", performerName.trimmed());
    ins.bindValue(":pdt", performedDate.toString(Qt::ISODate));
    ins.bindValue(":fmin", flightMinAtMaint);
    ins.bindValue(":cj", resultsJson);
    ins.bindValue(":nt", notes.trimmed());

    if (!ins.exec()) {
        if (errorOut) *errorOut = ins.lastError().text();
        m_db.rollback();
        return false;
    }
    const int recId = ins.lastInsertId().toInt();
    if (recordIdOut) *recordIdOut = recId;

    // Пересчитываем состояние борта по конечному автомату (учитывая открытые отказы и другие просроченные ТО)
    TransitionGuards guards;
    guards.hasOpenCriticalFaults = hasOpenCriticalFaults(uavId);
    guards.hasOverdueMaintenance = hasOverdueMaintenance(uavId, QDate::currentDate());

    if (auto next = UavStateMachine::transition(curStatus, UavEvent::CompleteMaintenance, guards)) {
        QSqlQuery upd(m_db.database());
        upd.prepare("UPDATE uavs SET state = :st WHERE id = :id;");
        upd.bindValue(":st", QString::fromStdString(toDbStatus(*next)));
        upd.bindValue(":id", uavId);
        if (!upd.exec()) {
            if (errorOut) *errorOut = upd.lastError().text();
            m_db.rollback();
            return false;
        }
    }

    AuditLogger::writeAuditRecord(
        m_db.database(), actor.id, "MAINT_COMPLETE", "maintenance_records", recId,
        QString::fromStdString(toDbStatus(curStatus)),
        QString("uav=%1;rule=%2;date=%3;min=%4").arg(uavId).arg(ruleId)
            .arg(performedDate.toString(Qt::ISODate)).arg(flightMinAtMaint));

    return m_db.commitQ(errorOut);
}

std::vector<FaultRecord> MaintenanceService::loadFaults(int uavId, bool onlyOpen) {
    std::vector<FaultRecord> list;
    QString sql = R"(
        SELECT f.id, f.uav_id, u.tail_number, f.reported_date, COALESCE(us.full_name,''),
               f.description, f.is_critical, COALESCE(f.resolved_date,''), COALESCE(f.resolution_works,'')
        FROM faults f
        JOIN uavs u ON u.id = f.uav_id
        LEFT JOIN users us ON us.id = f.reported_by
        WHERE 1=1
    )";
    if (uavId > 0) sql += " AND f.uav_id = :uid";
    if (onlyOpen) sql += " AND f.resolved_date IS NULL";
    sql += " ORDER BY (f.resolved_date IS NOT NULL) ASC, f.is_critical DESC, f.reported_date DESC, f.id DESC;";

    QSqlQuery q(m_db.database());
    q.prepare(sql);
    if (uavId > 0) q.bindValue(":uid", uavId);
    q.exec();
    while (q.next()) {
        FaultRecord fr;
        fr.id = q.value(0).toInt();
        fr.uavId = q.value(1).toInt();
        fr.uavTailNumber = q.value(2).toString().toStdString();
        fr.reportedDateIso = q.value(3).toString().toStdString();
        fr.reportedByName = q.value(4).toString().toStdString();
        fr.description = q.value(5).toString().toStdString();
        fr.isCritical = q.value(6).toInt() != 0;
        fr.resolvedDateIso = q.value(7).toString().toStdString();
        fr.resolutionWorks = q.value(8).toString().toStdString();
        list.push_back(std::move(fr));
    }
    return list;
}

bool MaintenanceService::reportFault(int uavId,
                                     const QDate& reportedDate,
                                     const QString& description,
                                     bool isCritical,
                                     const UserAccount& actor,
                                     int* faultIdOut,
                                     QString* errorOut) {
    try {
        RbacGuard::requirePermission(actor.role, Permission::ReportFault);
    } catch (const std::exception& ex) {
        if (errorOut) *errorOut = QString::fromUtf8(ex.what());
        return false;
    }

    if (uavId <= 0 || !reportedDate.isValid() || description.trimmed().isEmpty()) {
        if (errorOut) *errorOut = "Укажите борт, дату и описание неисправности";
        return false;
    }

    QSqlQuery uq(m_db.database());
    uq.prepare("SELECT state FROM uavs WHERE id = :id;");
    uq.bindValue(":id", uavId);
    if (!uq.exec() || !uq.next()) {
        if (errorOut) *errorOut = "Борт не найден";
        return false;
    }
    const UavStatus curStatus = fromDbStatus(uq.value(0).toString().toStdString());
    if (UavStateMachine::isTerminal(curStatus)) {
        if (errorOut) *errorOut = "Регистрация неисправности для списанного борта запрещена";
        return false;
    }

    if (!m_db.beginImmediateQ(errorOut)) return false;

    QSqlQuery ins(m_db.database());
    ins.prepare(R"(
        INSERT INTO faults (uav_id, reported_by, reported_date, description, is_critical)
        VALUES (:uid, :rb, :dt, :desc, :crit)
    )");
    ins.bindValue(":uid", uavId);
    ins.bindValue(":rb", actor.id);
    ins.bindValue(":dt", reportedDate.toString(Qt::ISODate));
    ins.bindValue(":desc", description.trimmed());
    ins.bindValue(":crit", isCritical ? 1 : 0);

    if (!ins.exec()) {
        if (errorOut) *errorOut = ins.lastError().text();
        m_db.rollback();
        return false;
    }
    const int faultId = ins.lastInsertId().toInt();
    if (faultIdOut) *faultIdOut = faultId;

    // REQ-FUNC-011: критичная неисправность немедленно переводит борт в состояние Faulty
    if (isCritical) {
        if (auto next = UavStateMachine::transition(curStatus, UavEvent::CriticalFaultReported)) {
            QSqlQuery upd(m_db.database());
            upd.prepare("UPDATE uavs SET state = :st WHERE id = :id;");
            upd.bindValue(":st", QString::fromStdString(toDbStatus(*next)));
            upd.bindValue(":id", uavId);
            if (!upd.exec()) {
                if (errorOut) *errorOut = upd.lastError().text();
                m_db.rollback();
                return false;
            }
        }
    }

    AuditLogger::writeAuditRecord(
        m_db.database(), actor.id, "FAULT_REPORT", "faults", faultId,
        QString::fromStdString(toDbStatus(curStatus)),
        QString("uav=%1;critical=%2;desc=%3").arg(uavId).arg(isCritical ? 1 : 0).arg(description.trimmed()));

    return m_db.commitQ(errorOut);
}

bool MaintenanceService::resolveFault(int faultId,
                                      const QDate& resolvedDate,
                                      const QString& resolutionWorks,
                                      const UserAccount& actor,
                                      QString* errorOut) {
    // REQ-FUNC-011 + REQ-SEC-002: отмечать устранение может только Техник или Администратор
    try {
        RbacGuard::requirePermission(actor.role, Permission::ResolveFault);
    } catch (const std::exception& ex) {
        if (errorOut) *errorOut = QString::fromUtf8(ex.what());
        return false;
    }

    if (!resolvedDate.isValid() || resolutionWorks.trimmed().isEmpty()) {
        if (errorOut) *errorOut = "Для отметки устранения обязательно укажите дату и описание выполненных ремонтных работ (REQ-FUNC-011)";
        return false;
    }

    QSqlQuery fq(m_db.database());
    fq.prepare("SELECT uav_id, resolved_date FROM faults WHERE id = :id;");
    fq.bindValue(":id", faultId);
    if (!fq.exec() || !fq.next()) {
        if (errorOut) *errorOut = "Запись о неисправности не найдена";
        return false;
    }
    if (!fq.value(1).isNull() && !fq.value(1).toString().isEmpty()) {
        if (errorOut) *errorOut = "Данная неисправность уже отмечена как устранённая";
        return false;
    }
    const int uavId = fq.value(0).toInt();

    QSqlQuery uq(m_db.database());
    uq.prepare("SELECT state FROM uavs WHERE id = :id;");
    uq.bindValue(":id", uavId);
    if (!uq.exec() || !uq.next()) {
        if (errorOut) *errorOut = "Связанный борт не найден";
        return false;
    }
    const UavStatus curStatus = fromDbStatus(uq.value(0).toString().toStdString());

    if (!m_db.beginImmediateQ(errorOut)) return false;

    QSqlQuery updF(m_db.database());
    updF.prepare("UPDATE faults SET resolved_date = :rd, resolution_works = :rw WHERE id = :id;");
    updF.bindValue(":rd", resolvedDate.toString(Qt::ISODate));
    updF.bindValue(":rw", resolutionWorks.trimmed());
    updF.bindValue(":id", faultId);
    if (!updF.exec()) {
        if (errorOut) *errorOut = updF.lastError().text();
        m_db.rollback();
        return false;
    }

    // Если все критичные неисправности устранены -> переводим Faulty -> Serviceable или DueMaintenance
    TransitionGuards guards;
    guards.hasOpenCriticalFaults = hasOpenCriticalFaults(uavId);
    guards.hasOverdueMaintenance = hasOverdueMaintenance(uavId, QDate::currentDate());

    if (curStatus == UavStatus::Faulty && !guards.hasOpenCriticalFaults) {
        if (auto next = UavStateMachine::transition(curStatus, UavEvent::AllCriticalFaultsResolved, guards)) {
            QSqlQuery updU(m_db.database());
            updU.prepare("UPDATE uavs SET state = :st WHERE id = :id;");
            updU.bindValue(":st", QString::fromStdString(toDbStatus(*next)));
            updU.bindValue(":id", uavId);
            if (!updU.exec()) {
                if (errorOut) *errorOut = updU.lastError().text();
                m_db.rollback();
                return false;
            }
        }
    }

    AuditLogger::writeAuditRecord(
        m_db.database(), actor.id, "FAULT_RESOLVE", "faults", faultId,
        "open", QString("resolved=%1;works=%2").arg(resolvedDate.toString(Qt::ISODate), resolutionWorks.trimmed()));

    return m_db.commitQ(errorOut);
}

} // namespace uavlog
