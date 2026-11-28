#include "services/FleetService.h"
#include "core/MaintenanceEngine.h"
#include "core/RbacGuard.h"
#include "core/UavStateMachine.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QSqlError>
#include <QSqlQuery>
#include <cmath>

namespace uavlog {

namespace {
int isoToJulian(const QString& isoDate) {
    const QDate d = QDate::fromString(isoDate, Qt::ISODate);
    return d.isValid() ? static_cast<int>(d.toJulianDay()) : 0;
}

std::vector<std::string> parseTasksJson(const QString& json) {
    std::vector<std::string> tasks;
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    if (doc.isArray()) {
        for (const auto& v : doc.array()) {
            const QString s = v.toString().trimmed();
            if (!s.isEmpty()) tasks.push_back(s.toStdString());
        }
    } else if (!json.trimmed().isEmpty()) {
        for (const auto& part : json.split(';', Qt::SkipEmptyParts)) {
            const QString s = part.trimmed();
            if (!s.isEmpty()) tasks.push_back(s.toStdString());
        }
    }
    return tasks;
}

QString serializeTasksJson(const std::vector<std::string>& tasks) {
    QJsonArray arr;
    for (const auto& t : tasks) {
        const QString s = QString::fromStdString(t).trimmed();
        if (!s.isEmpty()) arr.append(s);
    }
    return QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));
}
} // namespace

FleetService::FleetService(DatabaseManager& db, AuditLogger& audit)
    : m_db(db), m_audit(audit) {}

AlertThresholds FleetService::loadAlertThresholds() {
    AlertThresholds th;
    QSqlQuery q(m_db.database());
    q.exec("SELECT key, value FROM app_settings;");
    while (q.next()) {
        const QString k = q.value(0).toString();
        const QString v = q.value(1).toString();
        bool ok = false;
        if (k == "warn_maint_pct") {
            const double pct = v.toDouble(&ok);
            if (ok && pct > 0 && pct <= 100) th.maintHoursWarnRatio = pct / 100.0;
        } else if (k == "warn_maint_days") {
            const int d = v.toInt(&ok);
            if (ok && d >= 0) th.maintDaysWarn = d;
        } else if (k == "warn_permit_days") {
            const int d = v.toInt(&ok);
            if (ok && d >= 0) th.permitDaysWarn = d;
        } else if (k == "warn_battery_pct") {
            const double pct = v.toDouble(&ok);
            if (ok && pct > 0 && pct <= 100) th.batteryWarnRatio = pct / 100.0;
        }
    }
    return th;
}

bool FleetService::saveAlertThresholds(const AlertThresholds& th,
                                       const UserAccount& actor,
                                       QString* errorOut) {
    try {
        RbacGuard::requirePermission(actor.role, Permission::ManageUsersAndSettings);
    } catch (const std::exception& ex) {
        if (errorOut) *errorOut = QString::fromUtf8(ex.what());
        return false;
    }

    if (th.maintHoursWarnRatio <= 0 || th.maintHoursWarnRatio > 1.0 ||
        th.maintDaysWarn < 0 || th.permitDaysWarn < 0 ||
        th.batteryWarnRatio <= 0 || th.batteryWarnRatio > 1.0) {
        if (errorOut) *errorOut = "Некорректные значения порогов предупреждений";
        return false;
    }

    if (!m_db.beginImmediateQ(errorOut)) return false;

    auto setKV = [&](const QString& key, const QString& val) {
        QSqlQuery q(m_db.database());
        q.prepare("INSERT INTO app_settings (key, value) VALUES (:k, :v) "
                  "ON CONFLICT(key) DO UPDATE SET value = excluded.value;");
        q.bindValue(":k", key);
        q.bindValue(":v", val);
        return q.exec();
    };

    const int maintPct = static_cast<int>(std::round(th.maintHoursWarnRatio * 100.0));
    const int batPct = static_cast<int>(std::round(th.batteryWarnRatio * 100.0));
    if (!setKV("warn_maint_pct", QString::number(maintPct)) ||
        !setKV("warn_maint_days", QString::number(th.maintDaysWarn)) ||
        !setKV("warn_permit_days", QString::number(th.permitDaysWarn)) ||
        !setKV("warn_battery_pct", QString::number(batPct))) {
        if (errorOut) *errorOut = "Ошибка сохранения настроек порогов";
        m_db.rollback();
        return false;
    }

    AuditLogger::writeAuditRecord(
        m_db.database(), actor.id, "UPDATE", "app_settings", 0, "",
        QString("maint_pct=%1;maint_days=%2;permit_days=%3;bat_pct=%4")
            .arg(maintPct).arg(th.maintDaysWarn).arg(th.permitDaysWarn).arg(batPct));

    return m_db.commitQ(errorOut);
}

std::vector<UavType> FleetService::loadUavTypes() {
    std::vector<UavType> list;
    QSqlQuery q(m_db.database());
    q.exec("SELECT id, name, COALESCE(manufacturer,''), COALESCE(default_payload,''), COALESCE(notes,'') "
           "FROM uav_types ORDER BY name;");
    while (q.next()) {
        UavType t;
        t.id = q.value(0).toInt();
        t.name = q.value(1).toString().toStdString();
        t.manufacturer = q.value(2).toString().toStdString();
        t.defaultPayload = q.value(3).toString().toStdString();
        t.notes = q.value(4).toString().toStdString();
        list.push_back(std::move(t));
    }
    return list;
}

bool FleetService::saveUavType(const UavType& type, const UserAccount& actor, int* idOut, QString* errorOut) {
    try {
        RbacGuard::requirePermission(actor.role, Permission::ManageDictionaries);
    } catch (const std::exception& ex) {
        if (errorOut) *errorOut = QString::fromUtf8(ex.what());
        return false;
    }
    if (QString::fromStdString(type.name).trimmed().isEmpty()) {
        if (errorOut) *errorOut = "Наименование типа БЛА обязательно";
        return false;
    }
    if (!m_db.beginImmediateQ(errorOut)) return false;

    QSqlQuery q(m_db.database());
    int savedId = type.id;
    QString action = "CREATE";
    if (type.id <= 0) {
        q.prepare("INSERT INTO uav_types (name, manufacturer, default_payload, notes) VALUES (:n, :m, :p, :nt);");
    } else {
        action = "UPDATE";
        q.prepare("UPDATE uav_types SET name=:n, manufacturer=:m, default_payload=:p, notes=:nt WHERE id=:id;");
        q.bindValue(":id", type.id);
    }
    q.bindValue(":n", QString::fromStdString(type.name).trimmed());
    q.bindValue(":m", QString::fromStdString(type.manufacturer).trimmed());
    q.bindValue(":p", QString::fromStdString(type.defaultPayload).trimmed());
    q.bindValue(":nt", QString::fromStdString(type.notes).trimmed());

    if (!q.exec()) {
        if (errorOut) *errorOut = "Ошибка сохранения типа БЛА: " + q.lastError().text();
        m_db.rollback();
        return false;
    }
    if (savedId <= 0) savedId = q.lastInsertId().toInt();
    if (idOut) *idOut = savedId;

    AuditLogger::writeAuditRecord(m_db.database(), actor.id, action, "uav_types", savedId, "",
                                  QString::fromStdString(type.name));
    return m_db.commitQ(errorOut);
}

bool FleetService::deleteUavType(int typeId, const UserAccount& actor, QString* errorOut) {
    try {
        RbacGuard::requirePermission(actor.role, Permission::ManageDictionaries);
    } catch (const std::exception& ex) {
        if (errorOut) *errorOut = QString::fromUtf8(ex.what());
        return false;
    }
    QSqlQuery chk(m_db.database());
    chk.prepare("SELECT COUNT(*) FROM uavs WHERE uav_type_id = :id;");
    chk.bindValue(":id", typeId);
    if (chk.exec() && chk.next() && chk.value(0).toInt() > 0) {
        if (errorOut) *errorOut = "Нельзя удалить тип БЛА, к которому привязаны борта в реестре";
        return false;
    }
    if (!m_db.beginImmediateQ(errorOut)) return false;
    QSqlQuery del(m_db.database());
    del.prepare("DELETE FROM uav_types WHERE id = :id;");
    del.bindValue(":id", typeId);
    if (!del.exec()) {
        if (errorOut) *errorOut = del.lastError().text();
        m_db.rollback();
        return false;
    }
    AuditLogger::writeAuditRecord(m_db.database(), actor.id, "DELETE", "uav_types", typeId, "", "");
    return m_db.commitQ(errorOut);
}

std::vector<MaintenanceRule> FleetService::loadMaintenanceRules(int filterUavTypeId) {
    std::vector<MaintenanceRule> rules;
    QSqlQuery q(m_db.database());
    if (filterUavTypeId > 0) {
        q.prepare("SELECT id, uav_type_id, kind_name, interval_hours, interval_days, tasks_json "
                  "FROM maintenance_rules WHERE uav_type_id = :tid ORDER BY id;");
        q.bindValue(":tid", filterUavTypeId);
        q.exec();
    } else {
        q.exec("SELECT id, uav_type_id, kind_name, interval_hours, interval_days, tasks_json "
               "FROM maintenance_rules ORDER BY uav_type_id, id;");
    }
    while (q.next()) {
        MaintenanceRule r;
        r.id = q.value(0).toInt();
        r.uavTypeId = q.value(1).toInt();
        r.title = q.value(2).toString().toStdString();
        if (!q.value(3).isNull()) r.intervalHours = q.value(3).toDouble();
        if (!q.value(4).isNull()) r.intervalDays = q.value(4).toInt();
        r.tasks = parseTasksJson(q.value(5).toString());
        rules.push_back(std::move(r));
    }
    return rules;
}

bool FleetService::saveMaintenanceRule(const MaintenanceRule& rule,
                                       const UserAccount& actor,
                                       int* idOut,
                                       QString* errorOut) {
    try {
        RbacGuard::requirePermission(actor.role, Permission::ManageDictionaries);
    } catch (const std::exception& ex) {
        if (errorOut) *errorOut = QString::fromUtf8(ex.what());
        return false;
    }
    if (rule.uavTypeId <= 0 || QString::fromStdString(rule.title).trimmed().isEmpty()) {
        if (errorOut) *errorOut = "Укажите тип БЛА и наименование вида ТО";
        return false;
    }
    const bool hasHours = rule.intervalHours.has_value() && *rule.intervalHours > 0.0;
    const bool hasDays = rule.intervalDays.has_value() && *rule.intervalDays > 0;
    if (!hasHours && !hasDays) {
        // REQ-FUNC-002: вид ТО без обеих периодичностей не сохраняется
        if (errorOut) *errorOut = "Необходимо указать периодичность по налёту (ч) и/или по календарю (дни)";
        return false;
    }
    if (rule.tasks.empty()) {
        if (errorOut) *errorOut = "Укажите хотя бы один пункт перечня обязательных работ ТО";
        return false;
    }

    if (!m_db.beginImmediateQ(errorOut)) return false;

    QSqlQuery q(m_db.database());
    int savedId = rule.id;
    QString action = "CREATE";
    if (rule.id <= 0) {
        q.prepare("INSERT INTO maintenance_rules (uav_type_id, kind_name, interval_hours, interval_days, tasks_json) "
                  "VALUES (:tid, :kn, :ih, :idys, :tj);");
    } else {
        action = "UPDATE";
        q.prepare("UPDATE maintenance_rules SET uav_type_id=:tid, kind_name=:kn, interval_hours=:ih, "
                  "interval_days=:idys, tasks_json=:tj WHERE id=:id;");
        q.bindValue(":id", rule.id);
    }
    q.bindValue(":tid", rule.uavTypeId);
    q.bindValue(":kn", QString::fromStdString(rule.title).trimmed());
    q.bindValue(":ih", hasHours ? QVariant(*rule.intervalHours) : QVariant());
    q.bindValue(":idys", hasDays ? QVariant(*rule.intervalDays) : QVariant());
    q.bindValue(":tj", serializeTasksJson(rule.tasks));

    if (!q.exec()) {
        if (errorOut) *errorOut = q.lastError().text();
        m_db.rollback();
        return false;
    }
    if (savedId <= 0) savedId = q.lastInsertId().toInt();
    if (idOut) *idOut = savedId;

    AuditLogger::writeAuditRecord(m_db.database(), actor.id, action, "maintenance_rules", savedId, "",
                                  QString::fromStdString(rule.title));
    return m_db.commitQ(errorOut);
}

bool FleetService::deleteMaintenanceRule(int ruleId, const UserAccount& actor, QString* errorOut) {
    try {
        RbacGuard::requirePermission(actor.role, Permission::ManageDictionaries);
    } catch (const std::exception& ex) {
        if (errorOut) *errorOut = QString::fromUtf8(ex.what());
        return false;
    }
    QSqlQuery chk(m_db.database());
    chk.prepare("SELECT COUNT(*) FROM maintenance_records WHERE rule_id = :id;");
    chk.bindValue(":id", ruleId);
    if (chk.exec() && chk.next() && chk.value(0).toInt() > 0) {
        if (errorOut) *errorOut = "Нельзя удалить регламент ТО, по которому имеются записи выполнения";
        return false;
    }
    if (!m_db.beginImmediateQ(errorOut)) return false;
    QSqlQuery del(m_db.database());
    del.prepare("DELETE FROM maintenance_rules WHERE id = :id;");
    del.bindValue(":id", ruleId);
    if (!del.exec()) {
        if (errorOut) *errorOut = del.lastError().text();
        m_db.rollback();
        return false;
    }
    AuditLogger::writeAuditRecord(m_db.database(), actor.id, "DELETE", "maintenance_rules", ruleId, "", "");
    return m_db.commitQ(errorOut);
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
        s.performedDayNumber = isoToJulian(q.value(2).toString());
        recs.push_back(s);
    }
    return recs;
}

std::vector<Uav> FleetService::loadUavs(bool includeRetired) {
    std::vector<Uav> list;
    QSqlQuery q(m_db.database());
    const QString sql = includeRetired
        ? R"(SELECT u.id, u.tail_number, u.uav_type_id, t.name, u.serial_number,
                    u.commissioned_date, COALESCE(u.standard_payload,''), COALESCE(u.notes,''),
                    u.state, u.total_flight_min, u.total_flights, COALESCE(u.last_flight_date,'')
             FROM uavs u JOIN uav_types t ON t.id = u.uav_type_id ORDER BY u.tail_number;)"
        : R"(SELECT u.id, u.tail_number, u.uav_type_id, t.name, u.serial_number,
                    u.commissioned_date, COALESCE(u.standard_payload,''), COALESCE(u.notes,''),
                    u.state, u.total_flight_min, u.total_flights, COALESCE(u.last_flight_date,'')
             FROM uavs u JOIN uav_types t ON t.id = u.uav_type_id
             WHERE u.state != 'Retired' ORDER BY u.tail_number;)";
    q.exec(sql);
    while (q.next()) {
        Uav u;
        u.id = q.value(0).toInt();
        u.tailNumber = q.value(1).toString().toStdString();
        u.uavTypeId = q.value(2).toInt();
        u.typeName = q.value(3).toString().toStdString();
        u.serialNumber = q.value(4).toString().toStdString();
        u.commissionDateIso = q.value(5).toString().toStdString();
        u.standardPayload = q.value(6).toString().toStdString();
        u.notes = q.value(7).toString().toStdString();
        u.status = fromDbStatus(q.value(8).toString().toStdString());
        u.totalFlightMin = q.value(9).toInt();
        u.totalFlights = q.value(10).toInt();
        u.lastFlightDateIso = q.value(11).toString().toStdString();
        list.push_back(std::move(u));
    }
    return list;
}

bool FleetService::saveUav(const Uav& uav, const UserAccount& actor, int* idOut, QString* errorOut) {
    try {
        RbacGuard::requirePermission(actor.role, Permission::ManageDictionaries);
    } catch (const std::exception& ex) {
        if (errorOut) *errorOut = QString::fromUtf8(ex.what());
        return false;
    }

    const QString tail = QString::fromStdString(uav.tailNumber).trimmed();
    const QString serial = QString::fromStdString(uav.serialNumber).trimmed();
    const QString commDate = QString::fromStdString(uav.commissionDateIso).trimmed();
    if (tail.isEmpty() || serial.isEmpty() || uav.uavTypeId <= 0 || !QDate::fromString(commDate, Qt::ISODate).isValid()) {
        if (errorOut) *errorOut = "Заполните бортовой номер, тип БЛА, заводской номер и дату ввода";
        return false;
    }

    if (uav.id > 0) {
        // Списанный борт доступен только для чтения (REQ-FUNC-003-2)
        QSqlQuery st(m_db.database());
        st.prepare("SELECT state FROM uavs WHERE id = :id;");
        st.bindValue(":id", uav.id);
        if (st.exec() && st.next() && st.value(0).toString() == "Retired") {
            if (errorOut) *errorOut = "Списанный борт находится в архиве («только чтение») и не может быть изменён";
            return false;
        }
    }

    if (!m_db.beginImmediateQ(errorOut)) return false;

    QSqlQuery q(m_db.database());
    int savedId = uav.id;
    QString action = "CREATE";
    if (uav.id <= 0) {
        q.prepare(R"(
            INSERT INTO uavs (tail_number, uav_type_id, serial_number, commissioned_date,
                              state, standard_payload, notes)
            VALUES (:tn, :tid, :sn, :cd, 'Serviceable', :pl, :nt)
        )");
    } else {
        action = "UPDATE";
        q.prepare(R"(
            UPDATE uavs SET tail_number=:tn, uav_type_id=:tid, serial_number=:sn,
                            commissioned_date=:cd, standard_payload=:pl, notes=:nt
            WHERE id=:id
        )");
        q.bindValue(":id", uav.id);
    }
    q.bindValue(":tn", tail);
    q.bindValue(":tid", uav.uavTypeId);
    q.bindValue(":sn", serial);
    q.bindValue(":cd", commDate);
    q.bindValue(":pl", QString::fromStdString(uav.standardPayload).trimmed());
    q.bindValue(":nt", QString::fromStdString(uav.notes).trimmed());

    if (!q.exec()) {
        if (errorOut) *errorOut = "Ошибка сохранения борта (бортовой номер должен быть уникален): " + q.lastError().text();
        m_db.rollback();
        return false;
    }
    if (savedId <= 0) savedId = q.lastInsertId().toInt();
    if (idOut) *idOut = savedId;

    AuditLogger::writeAuditRecord(m_db.database(), actor.id, action, "uavs", savedId, "",
                                  QString("tail=%1;payload=%2").arg(tail, QString::fromStdString(uav.standardPayload)));
    return m_db.commitQ(errorOut);
}

bool FleetService::deleteUav(int uavId, const UserAccount& actor, QString* errorOut) {
    try {
        RbacGuard::requirePermission(actor.role, Permission::ManageDictionaries);
    } catch (const std::exception& ex) {
        if (errorOut) *errorOut = QString::fromUtf8(ex.what());
        return false;
    }

    // REQ-FUNC-003-2: физическое удаление борта при наличии истории строго запрещено!
    auto hasRows = [&](const QString& table) {
        QSqlQuery c(m_db.database());
        c.prepare(QString("SELECT COUNT(*) FROM %1 WHERE uav_id = :id;").arg(table));
        c.bindValue(":id", uavId);
        return c.exec() && c.next() && c.value(0).toInt() > 0;
    };
    if (hasRows("flights") || hasRows("maintenance_records") || hasRows("faults") || hasRows("preflight_checks")) {
        if (errorOut) {
            *errorOut = "Удаление запрещено: по борту имеются записи в журналах эксплуатации. "
                        "Используйте операцию «Списать» для перевода борта в архив (REQ-FUNC-003-2).";
        }
        return false;
    }

    if (!m_db.beginImmediateQ(errorOut)) return false;
    QSqlQuery del(m_db.database());
    del.prepare("DELETE FROM uavs WHERE id = :id;");
    del.bindValue(":id", uavId);
    if (!del.exec()) {
        if (errorOut) *errorOut = del.lastError().text();
        m_db.rollback();
        return false;
    }
    AuditLogger::writeAuditRecord(m_db.database(), actor.id, "DELETE", "uavs", uavId, "", "");
    return m_db.commitQ(errorOut);
}

bool FleetService::decommissionUav(int uavId, const UserAccount& actor, QString* errorOut) {
    try {
        RbacGuard::requirePermission(actor.role, Permission::DecommissionUav);
    } catch (const std::exception& ex) {
        if (errorOut) *errorOut = QString::fromUtf8(ex.what());
        return false;
    }

    QSqlQuery cur(m_db.database());
    cur.prepare("SELECT state, tail_number FROM uavs WHERE id = :id;");
    cur.bindValue(":id", uavId);
    if (!cur.exec() || !cur.next()) {
        if (errorOut) *errorOut = "Борт не найден";
        return false;
    }
    const UavStatus curStatus = fromDbStatus(cur.value(0).toString().toStdString());
    const QString tail = cur.value(1).toString();
    const auto nextStatus = UavStateMachine::transition(curStatus, UavEvent::Decommission);
    if (!nextStatus.has_value()) {
        if (errorOut) *errorOut = "Борт уже находится в архиве (списан)";
        return false;
    }

    if (!m_db.beginImmediateQ(errorOut)) return false;
    QSqlQuery upd(m_db.database());
    upd.prepare("UPDATE uavs SET state = 'Retired' WHERE id = :id;");
    upd.bindValue(":id", uavId);
    if (!upd.exec()) {
        if (errorOut) *errorOut = upd.lastError().text();
        m_db.rollback();
        return false;
    }
    // Открепляем закреплённые АКБ от списанного борта
    QSqlQuery unassign(m_db.database());
    unassign.prepare("UPDATE batteries SET assigned_uav_id = NULL WHERE assigned_uav_id = :id;");
    unassign.bindValue(":id", uavId);
    unassign.exec();

    AuditLogger::writeAuditRecord(m_db.database(), actor.id, "DECOMMISSION", "uavs", uavId,
                                  QString::fromStdString(toDbStatus(curStatus)),
                                  QString("Retired (%1)").arg(tail));
    return m_db.commitQ(errorOut);
}

std::vector<Battery> FleetService::loadBatteries(bool onlyAvailableForFlight) {
    std::vector<Battery> list;
    QSqlQuery q(m_db.database());
    // REQ-FUNC-016: АКБ с исчерпанным ресурсом (used_cycles >= max_cycles) не показываются при регистрации полёта
    const QString sql = onlyAvailableForFlight
        ? R"(SELECT b.id, b.inventory_no, b.model_name, b.commissioned_date, b.max_cycles,
                    b.used_cycles, b.assigned_uav_id, COALESCE(u.tail_number,''), b.retired
             FROM batteries b LEFT JOIN uavs u ON u.id = b.assigned_uav_id
             WHERE b.retired = 0 AND b.used_cycles < b.max_cycles
             ORDER BY b.inventory_no;)"
        : R"(SELECT b.id, b.inventory_no, b.model_name, b.commissioned_date, b.max_cycles,
                    b.used_cycles, b.assigned_uav_id, COALESCE(u.tail_number,''), b.retired
             FROM batteries b LEFT JOIN uavs u ON u.id = b.assigned_uav_id
             ORDER BY b.inventory_no;)";
    q.exec(sql);
    while (q.next()) {
        Battery b;
        b.id = q.value(0).toInt();
        b.inventoryNumber = q.value(1).toString().toStdString();
        b.modelName = q.value(2).toString().toStdString();
        b.commissionDateIso = q.value(3).toString().toStdString();
        b.maxCycles = q.value(4).toInt();
        b.usedCycles = q.value(5).toInt();
        if (!q.value(6).isNull()) b.assignedUavId = q.value(6).toInt();
        b.assignedUavTail = q.value(7).toString().toStdString();
        b.retired = q.value(8).toInt() != 0;
        list.push_back(std::move(b));
    }
    return list;
}

bool FleetService::saveBattery(const Battery& bat, const UserAccount& actor, int* idOut, QString* errorOut) {
    try {
        RbacGuard::requirePermission(actor.role, Permission::ManageDictionaries);
    } catch (const std::exception& ex) {
        if (errorOut) *errorOut = QString::fromUtf8(ex.what());
        return false;
    }
    const QString inv = QString::fromStdString(bat.inventoryNumber).trimmed();
    const QString model = QString::fromStdString(bat.modelName).trimmed();
    if (inv.isEmpty() || model.isEmpty() || bat.maxCycles <= 0) {
        if (errorOut) *errorOut = "Укажите инвентарный номер, модель АКБ и положительный лимит циклов";
        return false;
    }
    const QString commDate = bat.commissionDateIso.empty()
        ? QDate::currentDate().toString(Qt::ISODate)
        : QString::fromStdString(bat.commissionDateIso);

    if (!m_db.beginImmediateQ(errorOut)) return false;

    QSqlQuery q(m_db.database());
    int savedId = bat.id;
    QString action = "CREATE";
    if (bat.id <= 0) {
        q.prepare(R"(
            INSERT INTO batteries (inventory_no, model_name, max_cycles, used_cycles,
                                   assigned_uav_id, commissioned_date, retired)
            VALUES (:inv, :mod, :mc, 0, :auid, :cd, 0)
        )");
    } else {
        action = "UPDATE";
        q.prepare(R"(
            UPDATE batteries SET inventory_no=:inv, model_name=:mod, max_cycles=:mc,
                                 assigned_uav_id=:auid, commissioned_date=:cd, retired=:ret
            WHERE id=:id
        )");
        q.bindValue(":id", bat.id);
        q.bindValue(":ret", bat.retired ? 1 : 0);
    }
    q.bindValue(":inv", inv);
    q.bindValue(":mod", model);
    q.bindValue(":mc", bat.maxCycles);
    q.bindValue(":auid", bat.assignedUavId.has_value() && *bat.assignedUavId > 0
                             ? QVariant(*bat.assignedUavId)
                             : QVariant());
    q.bindValue(":cd", commDate);

    if (!q.exec()) {
        if (errorOut) *errorOut = "Ошибка сохранения АКБ (инвентарный номер должен быть уникален): " + q.lastError().text();
        m_db.rollback();
        return false;
    }
    if (savedId <= 0) savedId = q.lastInsertId().toInt();
    if (idOut) *idOut = savedId;

    AuditLogger::writeAuditRecord(m_db.database(), actor.id, action, "batteries", savedId, "",
                                  QString("inv=%1;max_cycles=%2").arg(inv).arg(bat.maxCycles));
    return m_db.commitQ(errorOut);
}

bool FleetService::retireBattery(int batteryId, const UserAccount& actor, QString* errorOut) {
    try {
        RbacGuard::requirePermission(actor.role, Permission::ManageDictionaries);
    } catch (const std::exception& ex) {
        if (errorOut) *errorOut = QString::fromUtf8(ex.what());
        return false;
    }
    if (!m_db.beginImmediateQ(errorOut)) return false;
    QSqlQuery q(m_db.database());
    q.prepare("UPDATE batteries SET retired = 1, assigned_uav_id = NULL WHERE id = :id;");
    q.bindValue(":id", batteryId);
    if (!q.exec()) {
        if (errorOut) *errorOut = q.lastError().text();
        m_db.rollback();
        return false;
    }
    AuditLogger::writeAuditRecord(m_db.database(), actor.id, "RETIRE", "batteries", batteryId, "", "retired=1");
    return m_db.commitQ(errorOut);
}

std::vector<OperatorInfo> FleetService::loadOperators(bool onlyActive) {
    std::vector<OperatorInfo> ops;
    QSqlQuery q(m_db.database());
    const QString sql = onlyActive
        ? "SELECT id, full_name, position, permit_number, permit_expiry, active FROM operators WHERE active = 1 ORDER BY full_name;"
        : "SELECT id, full_name, position, permit_number, permit_expiry, active FROM operators ORDER BY full_name;";
    q.exec(sql);
    while (q.next()) {
        OperatorInfo op;
        op.id = q.value(0).toInt();
        op.fullName = q.value(1).toString().toStdString();
        op.position = q.value(2).toString().toStdString();
        op.permitNumber = q.value(3).toString().toStdString();
        const QString expIso = q.value(4).toString();
        op.permitExpiryIso = expIso.toStdString();
        op.permitExpiryDayNumber = isoToJulian(expIso);
        op.active = q.value(5).toInt() != 0;

        QSqlQuery qt(m_db.database());
        qt.prepare("SELECT uav_type_id FROM operator_type_permits WHERE operator_id = :id ORDER BY uav_type_id;");
        qt.bindValue(":id", op.id);
        qt.exec();
        while (qt.next()) {
            op.allowedUavTypeIds.push_back(qt.value(0).toInt());
        }
        ops.push_back(std::move(op));
    }
    return ops;
}

bool FleetService::saveOperator(const OperatorInfo& op, const UserAccount& actor, int* idOut, QString* errorOut) {
    try {
        RbacGuard::requirePermission(actor.role, Permission::ManageOperators);
    } catch (const std::exception& ex) {
        if (errorOut) *errorOut = QString::fromUtf8(ex.what());
        return false;
    }

    const QString fullName = QString::fromStdString(op.fullName).trimmed();
    const QString position = QString::fromStdString(op.position).trimmed();
    const QString permitNo = QString::fromStdString(op.permitNumber).trimmed();
    const QString expiry = QString::fromStdString(op.permitExpiryIso).trimmed();

    // REQ-FUNC-005-2: оператор без даты окончания допуска не сохраняется
    if (fullName.isEmpty() || position.isEmpty() || permitNo.isEmpty() ||
        !QDate::fromString(expiry, Qt::ISODate).isValid()) {
        if (errorOut) *errorOut = "Заполните ФИО, должность, номер документа и корректную дату окончания допуска";
        return false;
    }
    if (op.allowedUavTypeIds.empty()) {
        if (errorOut) *errorOut = "Отметьте хотя бы один допущенный тип БЛА для оператора";
        return false;
    }

    if (!m_db.beginImmediateQ(errorOut)) return false;

    QSqlQuery q(m_db.database());
    int savedId = op.id;
    QString action = "CREATE";
    if (op.id <= 0) {
        q.prepare("INSERT INTO operators (full_name, position, permit_number, permit_expiry, active) "
                  "VALUES (:fn, :pos, :pn, :pe, 1);");
    } else {
        action = "UPDATE";
        q.prepare("UPDATE operators SET full_name=:fn, position=:pos, permit_number=:pn, "
                  "permit_expiry=:pe, active=:act WHERE id=:id;");
        q.bindValue(":id", op.id);
        q.bindValue(":act", op.active ? 1 : 0);
    }
    q.bindValue(":fn", fullName);
    q.bindValue(":pos", position);
    q.bindValue(":pn", permitNo);
    q.bindValue(":pe", expiry);

    if (!q.exec()) {
        if (errorOut) *errorOut = q.lastError().text();
        m_db.rollback();
        return false;
    }
    if (savedId <= 0) savedId = q.lastInsertId().toInt();
    if (idOut) *idOut = savedId;

    QSqlQuery delP(m_db.database());
    delP.prepare("DELETE FROM operator_type_permits WHERE operator_id = :id;");
    delP.bindValue(":id", savedId);
    delP.exec();

    const QString todayIso = QDate::currentDate().toString(Qt::ISODate);
    for (int tid : op.allowedUavTypeIds) {
        QSqlQuery insP(m_db.database());
        insP.prepare("INSERT OR IGNORE INTO operator_type_permits (operator_id, uav_type_id, granted_date) "
                     "VALUES (:oid, :tid, :gd);");
        insP.bindValue(":oid", savedId);
        insP.bindValue(":tid", tid);
        insP.bindValue(":gd", todayIso);
        insP.exec();
    }

    AuditLogger::writeAuditRecord(m_db.database(), actor.id, action, "operators", savedId, "",
                                  QString("name=%1;expiry=%2").arg(fullName, expiry));
    return m_db.commitQ(errorOut);
}

bool FleetService::setOperatorActive(int operatorId, bool active, const UserAccount& actor, QString* errorOut) {
    try {
        RbacGuard::requirePermission(actor.role, Permission::ManageOperators);
    } catch (const std::exception& ex) {
        if (errorOut) *errorOut = QString::fromUtf8(ex.what());
        return false;
    }
    if (!m_db.beginImmediateQ(errorOut)) return false;
    QSqlQuery q(m_db.database());
    q.prepare("UPDATE operators SET active = :a WHERE id = :id;");
    q.bindValue(":a", active ? 1 : 0);
    q.bindValue(":id", operatorId);
    if (!q.exec()) {
        if (errorOut) *errorOut = q.lastError().text();
        m_db.rollback();
        return false;
    }
    AuditLogger::writeAuditRecord(m_db.database(), actor.id, "UPDATE", "operators", operatorId, "",
                                  QString("active=%1").arg(active ? 1 : 0));
    return m_db.commitQ(errorOut);
}

int FleetService::recalculateCalendarDueStatuses(const QDate& today) {
    const auto rules = loadMaintenanceRules();
    const auto thresholds = loadAlertThresholds();
    const auto uavs = loadUavs(false);
    const int todayNum = static_cast<int>(today.toJulianDay());

    int transitioned = 0;
    for (const auto& u : uavs) {
        if (u.status != UavStatus::Serviceable) continue;
        const auto recs = loadLastMaintRecords(u.id);
        const auto rems = MaintenanceEngine::calculateRemainders(
            u, isoToJulian(QString::fromStdString(u.commissionDateIso)), todayNum, rules, recs, thresholds);
        bool anyOverdue = false;
        for (const auto& r : rems) {
            if (r.isOverdue) { anyOverdue = true; break; }
        }
        if (anyOverdue) {
            if (auto next = UavStateMachine::transition(u.status, UavEvent::MaintenanceThresholdReached)) {
                QSqlQuery upd(m_db.database());
                upd.prepare("UPDATE uavs SET state = :st WHERE id = :id;");
                upd.bindValue(":st", QString::fromStdString(toDbStatus(*next)));
                upd.bindValue(":id", u.id);
                if (upd.exec()) {
                    ++transitioned;
                }
            }
        }
    }
    return transitioned;
}

FleetDashboardData FleetService::loadDashboard(const QDate& today) {
    FleetDashboardData data;
    data.thresholds = loadAlertThresholds();
    const auto rules = loadMaintenanceRules();
    const int todayNum = static_cast<int>(today.toJulianDay());

    data.uavs = loadUavs(true);
    data.remainders.reserve(data.uavs.size());
    for (const auto& u : data.uavs) {
        const auto recs = loadLastMaintRecords(u.id);
        data.remainders.push_back(MaintenanceEngine::calculateRemainders(
            u, isoToJulian(QString::fromStdString(u.commissionDateIso)), todayNum, rules, recs, data.thresholds));
    }

    const auto ops = loadOperators(true);
    const auto bats = loadBatteries(false);
    data.alerts = MaintenanceEngine::buildAlerts(
        data.uavs, data.remainders, ops, bats, todayNum, data.thresholds);
    return data;
}

UavFormularData FleetService::loadFormular(int uavId, const QDate& today) {
    UavFormularData f;
    const auto dash = loadDashboard(today);
    for (std::size_t i = 0; i < dash.uavs.size(); ++i) {
        if (dash.uavs[i].id == uavId) {
            f.uav = dash.uavs[i];
            f.remainders = dash.remainders[i];
            break;
        }
    }

    // 1. Полёты борта
    {
        QSqlQuery q(m_db.database());
        q.prepare(R"(
            SELECT fl.id, fl.uav_id, u.tail_number, fl.operator_id, op.full_name,
                   fl.battery_id, b.inventory_no, fl.preflight_id, fl.flight_date,
                   fl.takeoff, fl.landing, fl.duration_min, COALESCE(fl.task_and_area,''),
                   COALESCE(fl.notes,''), COALESCE(fl.created_by, 0)
            FROM flights fl
            JOIN uavs u ON u.id = fl.uav_id
            JOIN operators op ON op.id = fl.operator_id
            JOIN batteries b ON b.id = fl.battery_id
            WHERE fl.uav_id = :uid
            ORDER BY fl.flight_date DESC, fl.id DESC
        )");
        q.bindValue(":uid", uavId);
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
            f.flights.push_back(std::move(r));
        }
    }

    // 2. Журнал ТО борта
    {
        QSqlQuery q(m_db.database());
        q.prepare(R"(
            SELECT mr.id, mr.uav_id, u.tail_number, mr.rule_id, r.kind_name,
                   mr.performed_date, mr.performer_name, mr.flight_min_at_maint,
                   mr.checklist_results_json, COALESCE(mr.notes,'')
            FROM maintenance_records mr
            JOIN uavs u ON u.id = mr.uav_id
            JOIN maintenance_rules r ON r.id = mr.rule_id
            WHERE mr.uav_id = :uid
            ORDER BY mr.performed_date DESC, mr.id DESC
        )");
        q.bindValue(":uid", uavId);
        q.exec();
        while (q.next()) {
            MaintenanceRecord m;
            m.id = q.value(0).toInt();
            m.uavId = q.value(1).toInt();
            m.uavTailNumber = q.value(2).toString().toStdString();
            m.ruleId = q.value(3).toInt();
            m.ruleTitle = q.value(4).toString().toStdString();
            m.performedDateIso = q.value(5).toString().toStdString();
            m.performer = q.value(6).toString().toStdString();
            m.flightMinAtMaint = q.value(7).toInt();
            m.checklistResults = q.value(8).toString().toStdString();
            m.notes = q.value(9).toString().toStdString();
            f.maintRecords.push_back(std::move(m));
        }
    }

    // 3. Неисправности борта
    {
        QSqlQuery q(m_db.database());
        q.prepare(R"(
            SELECT f.id, f.uav_id, u.tail_number, f.reported_date, COALESCE(us.full_name,''),
                   f.description, f.is_critical, COALESCE(f.resolved_date,''), COALESCE(f.resolution_works,'')
            FROM faults f
            JOIN uavs u ON u.id = f.uav_id
            LEFT JOIN users us ON us.id = f.reported_by
            WHERE f.uav_id = :uid
            ORDER BY f.reported_date DESC, f.id DESC
        )");
        q.bindValue(":uid", uavId);
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
            f.faults.push_back(std::move(fr));
        }
    }

    // 4. Закреплённые АКБ
    for (const auto& b : loadBatteries(false)) {
        if (b.assignedUavId.has_value() && *b.assignedUavId == uavId) {
            f.assignedBatteries.push_back(b);
        }
    }

    // 5. История предполётных осмотров (F-15)
    {
        QSqlQuery q(m_db.database());
        q.prepare(R"(
            SELECT pc.id, pc.uav_id, u.tail_number, pc.template_id, COALESCE(pc.inspector_id,0),
                   COALESCE(us.full_name,''), pc.check_dt, pc.passed, COALESCE(pc.notes,''),
                   (SELECT COUNT(*) FROM preflight_results pr
                    JOIN checklist_items ci ON ci.id = pr.item_id
                    WHERE pr.check_id = pc.id AND pr.is_ok = 0 AND ci.is_critical = 1)
            FROM preflight_checks pc
            JOIN uavs u ON u.id = pc.uav_id
            LEFT JOIN users us ON us.id = pc.inspector_id
            WHERE pc.uav_id = :uid
            ORDER BY pc.check_dt DESC, pc.id DESC
        )");
        q.bindValue(":uid", uavId);
        q.exec();
        while (q.next()) {
            PreflightCheckRecord rec;
            rec.id = q.value(0).toInt();
            rec.uavId = q.value(1).toInt();
            rec.uavTailNumber = q.value(2).toString().toStdString();
            rec.templateId = q.value(3).toInt();
            rec.inspectorId = q.value(4).toInt();
            rec.inspectorName = q.value(5).toString().toStdString();
            rec.checkDateIso = q.value(6).toString().toStdString();
            rec.passed = q.value(7).toInt() != 0;
            rec.notes = q.value(8).toString().toStdString();
            rec.criticalDefectsCount = q.value(9).toInt();
            f.preflightChecks.push_back(std::move(rec));
        }
    }

    return f;
}

} // namespace uavlog
