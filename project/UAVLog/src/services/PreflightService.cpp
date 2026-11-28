#include "services/PreflightService.h"
#include "core/RbacGuard.h"
#include "core/UavStateMachine.h"

#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>

namespace uavlog {

PreflightService::PreflightService(DatabaseManager& db, AuditLogger& audit)
    : m_db(db), m_audit(audit) {}

std::vector<ChecklistItem> PreflightService::loadItemsForTemplate(int templateId) {
    std::vector<ChecklistItem> items;
    QSqlQuery q(m_db.database());
    q.prepare("SELECT id, template_id, item_text, is_critical, sort_order "
              "FROM checklist_items WHERE template_id = :tid ORDER BY sort_order, id;");
    q.bindValue(":tid", templateId);
    q.exec();
    while (q.next()) {
        ChecklistItem it;
        it.id = q.value(0).toInt();
        it.templateId = q.value(1).toInt();
        it.itemText = q.value(2).toString().toStdString();
        it.isCritical = q.value(3).toInt() != 0;
        it.sortOrder = q.value(4).toInt();
        items.push_back(std::move(it));
    }
    return items;
}

std::vector<ChecklistTemplate> PreflightService::loadTemplates() {
    std::vector<ChecklistTemplate> list;
    QSqlQuery q(m_db.database());
    q.exec(R"(
        SELECT ct.id, ct.uav_type_id, t.name, ct.name, ct.active, ct.created_at
        FROM checklist_templates ct
        JOIN uav_types t ON t.id = ct.uav_type_id
        ORDER BY t.name, ct.id
    )");
    while (q.next()) {
        ChecklistTemplate tpl;
        tpl.id = q.value(0).toInt();
        tpl.uavTypeId = q.value(1).toInt();
        tpl.uavTypeName = q.value(2).toString().toStdString();
        tpl.name = q.value(3).toString().toStdString();
        tpl.active = q.value(4).toInt() != 0;
        tpl.createdAtIso = q.value(5).toString().toStdString();
        tpl.items = loadItemsForTemplate(tpl.id);
        list.push_back(std::move(tpl));
    }
    return list;
}

std::optional<ChecklistTemplate> PreflightService::loadActiveTemplateForUav(int uavId) {
    QSqlQuery q(m_db.database());
    q.prepare(R"(
        SELECT ct.id, ct.uav_type_id, t.name, ct.name, ct.active, ct.created_at
        FROM uavs u
        JOIN uav_types t ON t.id = u.uav_type_id
        JOIN checklist_templates ct ON ct.uav_type_id = u.uav_type_id AND ct.active = 1
        WHERE u.id = :uid
        ORDER BY ct.id DESC LIMIT 1
    )");
    q.bindValue(":uid", uavId);
    if (!q.exec() || !q.next()) {
        return std::nullopt;
    }

    ChecklistTemplate tpl;
    tpl.id = q.value(0).toInt();
    tpl.uavTypeId = q.value(1).toInt();
    tpl.uavTypeName = q.value(2).toString().toStdString();
    tpl.name = q.value(3).toString().toStdString();
    tpl.active = q.value(4).toInt() != 0;
    tpl.createdAtIso = q.value(5).toString().toStdString();
    tpl.items = loadItemsForTemplate(tpl.id);
    return tpl;
}

bool PreflightService::saveTemplate(const ChecklistTemplate& tpl,
                                    const UserAccount& actor,
                                    int* idOut,
                                    QString* errorOut) {
    try {
        RbacGuard::requirePermission(actor.role, Permission::ManageDictionaries);
    } catch (const std::exception& ex) {
        if (errorOut) *errorOut = QString::fromUtf8(ex.what());
        return false;
    }
    if (tpl.uavTypeId <= 0 || QString::fromStdString(tpl.name).trimmed().isEmpty()) {
        if (errorOut) *errorOut = "Укажите тип БЛА и наименование шаблона контрольной карты";
        return false;
    }

    if (!m_db.beginImmediateQ(errorOut)) return false;
    QSqlQuery q(m_db.database());
    int savedId = tpl.id;
    QString action = "CREATE";
    if (tpl.id <= 0) {
        q.prepare("INSERT INTO checklist_templates (uav_type_id, name, active, created_at) "
                  "VALUES (:tid, :nm, :act, :cr);");
        q.bindValue(":cr", QDate::currentDate().toString(Qt::ISODate));
    } else {
        action = "UPDATE";
        q.prepare("UPDATE checklist_templates SET uav_type_id=:tid, name=:nm, active=:act WHERE id=:id;");
        q.bindValue(":id", tpl.id);
    }
    q.bindValue(":tid", tpl.uavTypeId);
    q.bindValue(":nm", QString::fromStdString(tpl.name).trimmed());
    q.bindValue(":act", tpl.active ? 1 : 0);

    if (!q.exec()) {
        if (errorOut) *errorOut = q.lastError().text();
        m_db.rollback();
        return false;
    }
    if (savedId <= 0) savedId = q.lastInsertId().toInt();
    if (idOut) *idOut = savedId;

    AuditLogger::writeAuditRecord(m_db.database(), actor.id, action, "checklist_templates", savedId, "",
                                  QString::fromStdString(tpl.name));
    return m_db.commitQ(errorOut);
}

bool PreflightService::saveChecklistItem(const ChecklistItem& item,
                                         const UserAccount& actor,
                                         int* idOut,
                                         QString* errorOut) {
    try {
        RbacGuard::requirePermission(actor.role, Permission::ManageDictionaries);
    } catch (const std::exception& ex) {
        if (errorOut) *errorOut = QString::fromUtf8(ex.what());
        return false;
    }
    if (item.templateId <= 0 || QString::fromStdString(item.itemText).trimmed().isEmpty()) {
        if (errorOut) *errorOut = "Укажите текст пункта контрольной карты";
        return false;
    }

    if (!m_db.beginImmediateQ(errorOut)) return false;
    QSqlQuery q(m_db.database());
    int savedId = item.id;
    QString action = "CREATE";
    if (item.id <= 0) {
        q.prepare("INSERT INTO checklist_items (template_id, item_text, is_critical, sort_order) "
                  "VALUES (:tid, :txt, :crit, :ord);");
    } else {
        action = "UPDATE";
        q.prepare("UPDATE checklist_items SET item_text=:txt, is_critical=:crit, sort_order=:ord WHERE id=:id;");
        q.bindValue(":id", item.id);
    }
    q.bindValue(":tid", item.templateId);
    q.bindValue(":txt", QString::fromStdString(item.itemText).trimmed());
    q.bindValue(":crit", item.isCritical ? 1 : 0);
    q.bindValue(":ord", item.sortOrder);

    if (!q.exec()) {
        if (errorOut) *errorOut = q.lastError().text();
        m_db.rollback();
        return false;
    }
    if (savedId <= 0) savedId = q.lastInsertId().toInt();
    if (idOut) *idOut = savedId;

    AuditLogger::writeAuditRecord(m_db.database(), actor.id, action, "checklist_items", savedId, "",
                                  QString::fromStdString(item.itemText));
    return m_db.commitQ(errorOut);
}

bool PreflightService::deleteChecklistItem(int itemId, const UserAccount& actor, QString* errorOut) {
    try {
        RbacGuard::requirePermission(actor.role, Permission::ManageDictionaries);
    } catch (const std::exception& ex) {
        if (errorOut) *errorOut = QString::fromUtf8(ex.what());
        return false;
    }
    QSqlQuery chk(m_db.database());
    chk.prepare("SELECT COUNT(*) FROM preflight_results WHERE item_id = :id;");
    chk.bindValue(":id", itemId);
    if (chk.exec() && chk.next() && chk.value(0).toInt() > 0) {
        if (errorOut) *errorOut = "Пункт уже использован в истории осмотров и не может быть удалён";
        return false;
    }
    if (!m_db.beginImmediateQ(errorOut)) return false;
    QSqlQuery del(m_db.database());
    del.prepare("DELETE FROM checklist_items WHERE id = :id;");
    del.bindValue(":id", itemId);
    if (!del.exec()) {
        if (errorOut) *errorOut = del.lastError().text();
        m_db.rollback();
        return false;
    }
    AuditLogger::writeAuditRecord(m_db.database(), actor.id, "DELETE", "checklist_items", itemId, "", "");
    return m_db.commitQ(errorOut);
}

bool PreflightService::saveInspection(int uavId,
                                      int templateId,
                                      const QDate& checkDate,
                                      const std::vector<PreflightItemResult>& results,
                                      const QString& notes,
                                      bool createFaultOnCritical,
                                      const UserAccount& actor,
                                      bool* passedOut,
                                      int* checkIdOut,
                                      QString* errorOut) {
    try {
        RbacGuard::requirePermission(actor.role, Permission::RegisterPreflightCheck);
    } catch (const std::exception& ex) {
        if (errorOut) *errorOut = QString::fromUtf8(ex.what());
        return false;
    }
    if (uavId <= 0 || templateId <= 0 || !checkDate.isValid() || results.empty()) {
        if (errorOut) *errorOut = "Недостаточно данных для сохранения контрольной карты осмотра";
        return false;
    }

    QSqlQuery uq(m_db.database());
    uq.prepare("SELECT state FROM uavs WHERE id = :id;");
    uq.bindValue(":id", uavId);
    if (!uq.exec() || !uq.next()) {
        if (errorOut) *errorOut = "Борт не найден";
        return false;
    }
    const UavStatus currentStatus = fromDbStatus(uq.value(0).toString().toStdString());
    if (UavStateMachine::isTerminal(currentStatus)) {
        if (errorOut) *errorOut = "Нельзя провести предполётный осмотр для списанного борта";
        return false;
    }

    bool hasCriticalDefect = false;
    QStringList criticalDefectTexts;
    for (const auto& r : results) {
        if (!r.isOk && r.isCritical) {
            hasCriticalDefect = true;
            QString desc = QString::fromStdString(r.itemText);
            if (!r.comment.empty()) {
                desc += " (" + QString::fromStdString(r.comment) + ")";
            }
            criticalDefectTexts << desc;
        }
    }
    const bool passed = !hasCriticalDefect;
    if (passedOut) *passedOut = passed;

    if (!m_db.beginImmediateQ(errorOut)) return false;

    QSqlQuery insCheck(m_db.database());
    insCheck.prepare(R"(
        INSERT INTO preflight_checks (uav_id, template_id, inspector_id, check_dt, passed, notes)
        VALUES (:uid, :tid, :iid, :dt, :pass, :nt)
    )");
    insCheck.bindValue(":uid", uavId);
    insCheck.bindValue(":tid", templateId);
    insCheck.bindValue(":iid", actor.id);
    insCheck.bindValue(":dt", checkDate.toString(Qt::ISODate));
    insCheck.bindValue(":pass", passed ? 1 : 0);
    insCheck.bindValue(":nt", notes.trimmed());

    if (!insCheck.exec()) {
        if (errorOut) *errorOut = insCheck.lastError().text();
        m_db.rollback();
        return false;
    }
    const int checkId = insCheck.lastInsertId().toInt();
    if (checkIdOut) *checkIdOut = checkId;

    for (const auto& r : results) {
        QSqlQuery insRes(m_db.database());
        insRes.prepare(R"(
            INSERT INTO preflight_results (check_id, item_id, is_ok, comment)
            VALUES (:cid, :iid, :ok, :cm)
        )");
        insRes.bindValue(":cid", checkId);
        insRes.bindValue(":iid", r.itemId);
        insRes.bindValue(":ok", r.isOk ? 1 : 0);
        insRes.bindValue(":cm", QString::fromStdString(r.comment).trimmed());
        if (!insRes.exec()) {
            if (errorOut) *errorOut = insRes.lastError().text();
            m_db.rollback();
            return false;
        }
    }

    if (passed) {
        AuditLogger::writeAuditRecord(m_db.database(), actor.id, "PREFLIGHT_PASS", "preflight_checks",
                                      checkId, "", QString("uav=%1;date=%2").arg(uavId).arg(checkDate.toString(Qt::ISODate)));
    } else {
        // При критичном замечании конечный автомат переводит борт в Faulty (SDD DV-05, ADR-0006)
        if (auto next = UavStateMachine::transition(currentStatus, UavEvent::CriticalFaultReported)) {
            QSqlQuery updUav(m_db.database());
            updUav.prepare("UPDATE uavs SET state = :st WHERE id = :id;");
            updUav.bindValue(":st", QString::fromStdString(toDbStatus(*next)));
            updUav.bindValue(":id", uavId);
            if (!updUav.exec()) {
                if (errorOut) *errorOut = updUav.lastError().text();
                m_db.rollback();
                return false;
            }
        }
        if (createFaultOnCritical) {
            QSqlQuery insFault(m_db.database());
            insFault.prepare(R"(
                INSERT INTO faults (uav_id, reported_by, reported_date, description, is_critical)
                VALUES (:uid, :rb, :dt, :desc, 1)
            )");
            insFault.bindValue(":uid", uavId);
            insFault.bindValue(":rb", actor.id);
            insFault.bindValue(":dt", checkDate.toString(Qt::ISODate));
            insFault.bindValue(":desc", "Предполётный осмотр (F-15): " + criticalDefectTexts.join("; "));
            insFault.exec();
        }
        AuditLogger::writeAuditRecord(m_db.database(), actor.id, "PREFLIGHT_FAIL_FAULTY", "preflight_checks",
                                      checkId, QString::fromStdString(toDbStatus(currentStatus)),
                                      QString("Faulty;defects=%1").arg(criticalDefectTexts.join("; ")));
    }

    return m_db.commitQ(errorOut);
}

std::vector<FleetReadinessRow> PreflightService::buildFleetReadinessRows(const QDate& date) {
    std::vector<FleetReadinessRow> rows;
    const QString dateIso = date.toString(Qt::ISODate);

    QSqlQuery q(m_db.database());
    q.prepare(R"(
        SELECT u.id, u.tail_number, t.name, u.state,
               (SELECT pc.passed FROM preflight_checks pc
                WHERE pc.uav_id = u.id AND pc.check_dt = :d1 ORDER BY pc.id DESC LIMIT 1) AS pf_passed,
               (SELECT COALESCE(us.full_name,'') FROM preflight_checks pc
                LEFT JOIN users us ON us.id = pc.inspector_id
                WHERE pc.uav_id = u.id AND pc.check_dt = :d2 ORDER BY pc.id DESC LIMIT 1) AS pf_inspector,
               (SELECT COALESCE(pc.notes,'') FROM preflight_checks pc
                WHERE pc.uav_id = u.id AND pc.check_dt = :d3 ORDER BY pc.id DESC LIMIT 1) AS pf_notes
        FROM uavs u
        JOIN uav_types t ON t.id = u.uav_type_id
        WHERE u.state != 'Retired'
        ORDER BY u.tail_number
    )");
    q.bindValue(":d1", dateIso);
    q.bindValue(":d2", dateIso);
    q.bindValue(":d3", dateIso);
    q.exec();

    while (q.next()) {
        FleetReadinessRow r;
        r.uavId = q.value(0).toInt();
        r.tailNumber = q.value(1).toString();
        r.uavType = q.value(2).toString();
        const UavStatus st = fromDbStatus(q.value(3).toString().toStdString());
        r.uavStatusRu = QString::fromStdString(toRussianStatus(st));

        const bool hasCheck = !q.value(4).isNull();
        const bool passed = hasCheck && q.value(4).toInt() != 0;
        r.inspectorName = hasCheck ? q.value(5).toString() : "—";
        r.defectsSummary = hasCheck ? q.value(6).toString() : "Осмотр на дату не проводился";

        if (!hasCheck) {
            r.preflightStatusRu = "Не проведён";
        } else if (passed) {
            r.preflightStatusRu = "Пройден";
        } else {
            r.preflightStatusRu = "Критичные замечания";
        }

        r.readyForFlight = (st == UavStatus::Serviceable) && hasCheck && passed;
        rows.push_back(std::move(r));
    }

    return rows;
}

} // namespace uavlog
