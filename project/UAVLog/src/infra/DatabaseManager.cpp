#include "infra/DatabaseManager.h"
#include "infra/CryptoProvider.h"

#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>

namespace uavlog {

DatabaseManager::DatabaseManager(const QString& connectionName)
    : m_connectionName(connectionName) {}

DatabaseManager::~DatabaseManager() {
    if (m_db.isOpen()) {
        m_db.close();
    }
    m_db = QSqlDatabase();
    if (QSqlDatabase::contains(m_connectionName)) {
        QSqlDatabase::removeDatabase(m_connectionName);
    }
}

bool DatabaseManager::openAndInitialize(const QString& dbFilePath, QString* errorOut) {
    m_dbPath = dbFilePath;
    if (dbFilePath != ":memory:") {
        QDir().mkpath(QFileInfo(dbFilePath).absolutePath());
    }

    if (QSqlDatabase::contains(m_connectionName)) {
        m_db = QSqlDatabase::database(m_connectionName);
    } else {
        m_db = QSqlDatabase::addDatabase("QSQLITE", m_connectionName);
    }
    m_db.setDatabaseName(dbFilePath);

    if (!m_db.open()) {
        if (errorOut) *errorOut = m_db.lastError().text();
        return false;
    }

    QSqlQuery pragma(m_db);
    pragma.exec("PRAGMA foreign_keys = ON;");
    if (dbFilePath != ":memory:") {
        pragma.exec("PRAGMA journal_mode = WAL;");
        pragma.exec("PRAGMA synchronous = FULL;");
    }

    if (!checkIntegrity(errorOut)) {
        return false;
    }

    return applySchema(errorOut);
}

bool DatabaseManager::checkIntegrity(QString* errorOut) {
    QSqlQuery q(m_db);
    if (!q.exec("PRAGMA integrity_check;") || !q.next()) {
        if (errorOut) *errorOut = "Ошибка выполнения PRAGMA integrity_check";
        return false;
    }
    const QString result = q.value(0).toString();
    if (result.compare("ok", Qt::CaseInsensitive) != 0) {
        if (errorOut) *errorOut = "Повреждение целостности БД: " + result;
        return false;
    }
    return true;
}

bool DatabaseManager::createBackupVacuumInto(const QString& targetFilePath, QString* errorOut) {
    QDir().mkpath(QFileInfo(targetFilePath).absolutePath());
    QString escaped = targetFilePath;
    escaped.replace('\'', "''");
    QSqlQuery q(m_db);
    if (!q.exec(QString("VACUUM INTO '%1';").arg(escaped))) {
        if (errorOut) *errorOut = q.lastError().text();
        return false;
    }
    return true;
}

bool DatabaseManager::beginImmediate(QString* errorOut) {
    QSqlQuery q(m_db);
    if (!q.exec("BEGIN IMMEDIATE;")) {
        if (errorOut) *errorOut = q.lastError().text();
        return false;
    }
    return true;
}

bool DatabaseManager::commit(QString* errorOut) {
    QSqlQuery q(m_db);
    if (!q.exec("COMMIT;")) {
        if (errorOut) *errorOut = q.lastError().text();
        return false;
    }
    return true;
}

void DatabaseManager::rollback() noexcept {
    QSqlQuery q(m_db);
    q.exec("ROLLBACK;");
}

bool DatabaseManager::applySchema(QString* errorOut) {
    // Полная схема БД согласно SDD DV-04 и ADR-0002 / ADR-0003 / ADR-0006
    const QStringList statements = {
        R"(CREATE TABLE IF NOT EXISTS users (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            username TEXT NOT NULL UNIQUE,
            full_name TEXT NOT NULL,
            role TEXT NOT NULL CHECK(role IN ('Administrator','Technician','Operator','Observer')),
            password_hash TEXT NOT NULL,
            salt TEXT NOT NULL,
            iterations INTEGER NOT NULL DEFAULT 100000,
            failed_attempts INTEGER NOT NULL DEFAULT 0,
            locked_until TEXT,
            active INTEGER NOT NULL DEFAULT 1
        );)",

        R"(CREATE TABLE IF NOT EXISTS uav_types (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL UNIQUE,
            description TEXT
        );)",

        R"(CREATE TABLE IF NOT EXISTS maintenance_rules (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            uav_type_id INTEGER NOT NULL REFERENCES uav_types(id) ON DELETE CASCADE,
            title TEXT NOT NULL,
            interval_hours REAL,
            interval_days INTEGER,
            works_checklist TEXT NOT NULL,
            CHECK(interval_hours IS NOT NULL OR interval_days IS NOT NULL)
        );)",

        R"(CREATE TABLE IF NOT EXISTS uavs (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            tail_number TEXT NOT NULL UNIQUE,
            uav_type_id INTEGER NOT NULL REFERENCES uav_types(id),
            serial_number TEXT NOT NULL,
            commission_date TEXT NOT NULL,
            standard_payload TEXT,
            status TEXT NOT NULL DEFAULT 'Serviceable'
                CHECK(status IN ('Serviceable','DueMaintenance','InMaintenance','Faulty','Retired')),
            total_flight_min INTEGER NOT NULL DEFAULT 0,
            total_flights INTEGER NOT NULL DEFAULT 0,
            last_flight_date TEXT
        );)",

        R"(CREATE TABLE IF NOT EXISTS batteries (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            inventory_number TEXT NOT NULL UNIQUE,
            type_name TEXT NOT NULL,
            commission_date TEXT NOT NULL,
            max_cycles INTEGER NOT NULL CHECK(max_cycles > 0),
            used_cycles INTEGER NOT NULL DEFAULT 0,
            assigned_uav_id INTEGER REFERENCES uavs(id),
            retired INTEGER NOT NULL DEFAULT 0
        );)",

        R"(CREATE TABLE IF NOT EXISTS operators (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            full_name TEXT NOT NULL,
            position TEXT NOT NULL,
            permit_number TEXT NOT NULL,
            permit_expiry_date TEXT NOT NULL
        );)",

        R"(CREATE TABLE IF NOT EXISTS operator_uav_types (
            operator_id INTEGER NOT NULL REFERENCES operators(id) ON DELETE CASCADE,
            uav_type_id INTEGER NOT NULL REFERENCES uav_types(id) ON DELETE CASCADE,
            PRIMARY KEY (operator_id, uav_type_id)
        );)",

        R"(CREATE TABLE IF NOT EXISTS flights (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            uav_id INTEGER NOT NULL REFERENCES uavs(id),
            operator_id INTEGER NOT NULL REFERENCES operators(id),
            battery_id INTEGER NOT NULL REFERENCES batteries(id),
            flight_date TEXT NOT NULL,
            takeoff_time TEXT NOT NULL,
            landing_time TEXT NOT NULL,
            duration_min INTEGER NOT NULL CHECK(duration_min > 0 AND duration_min <= 720),
            task_and_area TEXT,
            notes TEXT,
            created_by INTEGER REFERENCES users(id)
        );)",

        R"(CREATE TABLE IF NOT EXISTS maintenance_records (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            uav_id INTEGER NOT NULL REFERENCES uavs(id),
            rule_id INTEGER NOT NULL REFERENCES maintenance_rules(id),
            performed_date TEXT NOT NULL,
            performer TEXT NOT NULL,
            flight_min_at_maint INTEGER NOT NULL,
            notes TEXT
        );)",

        R"(CREATE TABLE IF NOT EXISTS faults (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            uav_id INTEGER NOT NULL REFERENCES uavs(id),
            reported_date TEXT NOT NULL,
            description TEXT NOT NULL,
            is_critical INTEGER NOT NULL DEFAULT 0,
            resolved_date TEXT,
            resolution_works TEXT
        );)",

        R"(CREATE TABLE IF NOT EXISTS checklist_templates (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            uav_type_id INTEGER NOT NULL REFERENCES uav_types(id) ON DELETE CASCADE,
            title TEXT NOT NULL
        );)",

        R"(CREATE TABLE IF NOT EXISTS checklist_items (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            template_id INTEGER NOT NULL REFERENCES checklist_templates(id) ON DELETE CASCADE,
            item_text TEXT NOT NULL,
            is_critical INTEGER NOT NULL DEFAULT 1
        );)",

        R"(CREATE TABLE IF NOT EXISTS preflight_checks (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            uav_id INTEGER NOT NULL REFERENCES uavs(id),
            check_date TEXT NOT NULL,
            inspector_user_id INTEGER REFERENCES users(id),
            passed INTEGER NOT NULL DEFAULT 0,
            notes TEXT
        );)",

        R"(CREATE TABLE IF NOT EXISTS preflight_results (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            check_id INTEGER NOT NULL REFERENCES preflight_checks(id) ON DELETE CASCADE,
            item_id INTEGER NOT NULL REFERENCES checklist_items(id),
            is_ok INTEGER NOT NULL DEFAULT 1,
            comment TEXT
        );)",

        R"(CREATE TABLE IF NOT EXISTS audit_log (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id INTEGER,
            created_at TEXT NOT NULL,
            action TEXT NOT NULL,
            entity TEXT NOT NULL,
            entity_id INTEGER,
            old_value TEXT,
            new_value TEXT
        );)",

        R"(CREATE TABLE IF NOT EXISTS app_settings (
            key TEXT PRIMARY KEY,
            value TEXT NOT NULL
        );)",

        // Составные индексы согласно SDD DV-04 (REQ-PERF-001..003)
        "CREATE INDEX IF NOT EXISTS idx_flights_uav_date ON flights(uav_id, flight_date DESC);",
        "CREATE INDEX IF NOT EXISTS idx_flights_op_date ON flights(operator_id, flight_date DESC);",
        "CREATE INDEX IF NOT EXISTS idx_maint_uav_rule ON maintenance_records(uav_id, rule_id, performed_date DESC);",
        "CREATE INDEX IF NOT EXISTS idx_faults_uav_open ON faults(uav_id, is_critical, resolved_date);",
        "CREATE INDEX IF NOT EXISTS idx_audit_dt ON audit_log(created_at DESC);",
        "PRAGMA user_version = 1;"
    };

    QSqlQuery q(m_db);
    for (const auto& sql : statements) {
        if (!q.exec(sql)) {
            if (errorOut) *errorOut = q.lastError().text();
            return false;
        }
    }
    return true;
}

void DatabaseManager::seedDemoDataIfEmpty() {
    QSqlQuery check(m_db);
    if (!check.exec("SELECT COUNT(*) FROM users;") || !check.next() || check.value(0).toInt() > 0) {
        return;
    }

    auto insertUser = [&](const QString& user, const QString& full, const QString& role, const QString& pass) {
        const QString salt = CryptoProvider::generateSaltHex();
        const QString hash = CryptoProvider::hashPasswordPbkdf2(pass, salt);
        QSqlQuery q(m_db);
        q.prepare(R"(
            INSERT INTO users (username, full_name, role, password_hash, salt, iterations)
            VALUES (:u, :f, :r, :h, :s, :it)
        )");
        q.bindValue(":u", user);
        q.bindValue(":f", full);
        q.bindValue(":r", role);
        q.bindValue(":h", hash);
        q.bindValue(":s", salt);
        q.bindValue(":it", CryptoProvider::kDefaultIterations);
        q.exec();
    };

    // 4 эталонные учётные записи по ролям SRS 2.4
    insertUser("admin",    "Мороз А. А. (Нач. службы)", "Administrator", "Admin1234!");
    insertUser("tech",     "Ковалёв И. В. (Техник ТО)", "Technician",    "Tech1234!");
    insertUser("operator", "Соколов Д. С. (Оператор)",  "Operator",      "Oper1234!");
    insertUser("head",     "Волков П. Н. (Руководитель)","Observer",     "Head1234!");

    QSqlQuery q(m_db);
    // Типы БЛА и регламенты ТО (F-03)
    q.exec("INSERT INTO uav_types (id, name, description) VALUES (1, 'Квадрокоптер М-30', 'Малый разведывательный квадрокоптер');");
    q.exec("INSERT INTO uav_types (id, name, description) VALUES (2, 'Крыло С-10', 'БЛА самолётного типа дальнего радиуса');");

    q.exec("INSERT INTO maintenance_rules (id, uav_type_id, title, interval_hours, interval_days, works_checklist) VALUES "
           "(1, 1, 'ТО-25 (оперативное)', 25.0, 30, 'Осмотр винтов; Проверка люфта моторов; Калибровка IMU');");
    q.exec("INSERT INTO maintenance_rules (id, uav_type_id, title, interval_hours, interval_days, works_checklist) VALUES "
           "(2, 1, 'ТО-100 (периодическое)', 100.0, 180, 'Замена подшипников; Диагностика регуляторов; Проверка антенн');");
    q.exec("INSERT INTO maintenance_rules (id, uav_type_id, title, interval_hours, interval_days, works_checklist) VALUES "
           "(3, 2, 'ТО-50 (планер и СУ)', 50.0, 60, 'Осмотр консолей крыла; Проверка сервоприводов; Проверка парашюта');");

    const QString today = QDate::currentDate().toString(Qt::ISODate);
    const QString daysAgo20 = QDate::currentDate().addDays(-20).toString(Qt::ISODate);
    const QString daysAgo45 = QDate::currentDate().addDays(-45).toString(Qt::ISODate);
    const QString in90Days  = QDate::currentDate().addDays(90).toString(Qt::ISODate);
    const QString in10Days  = QDate::currentDate().addDays(10).toString(Qt::ISODate);
    const QString expired5  = QDate::currentDate().addDays(-5).toString(Qt::ISODate);

    // Парк БЛА (4 борта в разных состояниях для демонстрации индикации и предупреждений)
    q.exec(QString("INSERT INTO uavs (id, tail_number, uav_type_id, serial_number, commission_date, standard_payload, status, total_flight_min, total_flights, last_flight_date) VALUES "
                   "(1, 'БЛА-01', 1, 'SN-M30-001', '%1', 'ОЭ-камера 4K / тепловизор', 'Serviceable', 1410, 28, '%2');")
               .arg(daysAgo20, today));
    q.exec(QString("INSERT INTO uavs (id, tail_number, uav_type_id, serial_number, commission_date, standard_payload, status, total_flight_min, total_flights, last_flight_date) VALUES "
                   "(2, 'БЛА-02', 1, 'SN-M30-002', '%1', 'ОЭ-камера 4K', 'Serviceable', 620, 14, '%2');")
               .arg(daysAgo20, today));
    q.exec(QString("INSERT INTO uavs (id, tail_number, uav_type_id, serial_number, commission_date, standard_payload, status, total_flight_min, total_flights, last_flight_date) VALUES "
                   "(3, 'БЛА-03', 2, 'SN-S10-007', '%1', 'Фотограмметрия 42 Мп', 'DueMaintenance', 3060, 41, '%2');")
               .arg(daysAgo45, daysAgo20));
    q.exec(QString("INSERT INTO uavs (id, tail_number, uav_type_id, serial_number, commission_date, standard_payload, status, total_flight_min, total_flights, last_flight_date) VALUES "
                   "(4, 'БЛА-04', 2, 'SN-S10-009', '%1', 'Ретранслятор связи', 'Faulty', 980, 12, '%2');")
               .arg(daysAgo45, daysAgo20));

    // АКБ с разной выработкой ресурса
    q.exec(QString("INSERT INTO batteries (id, inventory_number, type_name, commission_date, max_cycles, used_cycles, assigned_uav_id) VALUES "
                   "(1, 'АКБ-101', 'LiPo 6S 5880mAh', '%1', 200, 45, 1);").arg(daysAgo20));
    q.exec(QString("INSERT INTO batteries (id, inventory_number, type_name, commission_date, max_cycles, used_cycles, assigned_uav_id) VALUES "
                   "(2, 'АКБ-102', 'LiPo 6S 5880mAh', '%1', 200, 184, 2);").arg(daysAgo45));
    q.exec(QString("INSERT INTO batteries (id, inventory_number, type_name, commission_date, max_cycles, used_cycles, assigned_uav_id) VALUES "
                   "(3, 'АКБ-103', 'LiPo 6S 5880mAh', '%1', 200, 200, NULL);").arg(daysAgo45));

    // Операторы и допуски
    q.exec(QString("INSERT INTO operators (id, full_name, position, permit_number, permit_expiry_date) VALUES "
                   "(1, 'Соколов Д. С.', 'Старший оператор БЛА', 'ДОП-2026-01', '%1');").arg(in90Days));
    q.exec(QString("INSERT INTO operators (id, full_name, position, permit_number, permit_expiry_date) VALUES "
                   "(2, 'Петров А. И.', 'Оператор БЛА', 'ДОП-2026-04', '%1');").arg(in10Days));
    q.exec(QString("INSERT INTO operators (id, full_name, position, permit_number, permit_expiry_date) VALUES "
                   "(3, 'Громов В. К.', 'Оператор БЛА', 'ДОП-2025-19', '%1');").arg(expired5));

    q.exec("INSERT INTO operator_uav_types VALUES (1, 1), (1, 2), (2, 1), (3, 1);");

    // Шаблоны предполётного чек-листа (F-15) и пройденный осмотр на сегодня для БЛА-01 и БЛА-02
    q.exec("INSERT INTO checklist_templates (id, uav_type_id, title) VALUES (1, 1, 'Контрольная карта М-30');");
    q.exec("INSERT INTO checklist_items (id, template_id, item_text, is_critical) VALUES "
           "(1, 1, 'Целостность лопастей и фиксация винтов', 1),"
           "(2, 1, 'Надёжность фиксации АКБ и заряд ≥ 90 %', 1),"
           "(3, 1, 'Стабильный приём GNSS и телеметрии', 1);");
    q.exec(QString("INSERT INTO preflight_checks (id, uav_id, check_date, inspector_user_id, passed, notes) VALUES "
                   "(1, 1, '%1', 2, 1, 'Осмотр выполнен, замечаний нет'),"
                   "(2, 2, '%1', 2, 0, 'Выявлен люфт крепления переднего луча');").arg(today));

    // Настройки порогов по умолчанию (REQ-FUNC-009-2)
    q.exec("INSERT OR IGNORE INTO app_settings (key, value) VALUES "
           "('warn_maint_pct', '10'), ('warn_maint_days', '7'), "
           "('warn_permit_days', '14'), ('warn_battery_pct', '90');");
}

} // namespace uavlog
