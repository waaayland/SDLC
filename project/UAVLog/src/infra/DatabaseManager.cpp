#include "infra/DatabaseManager.h"
#include "infra/AuditLogger.h"
#include "infra/CryptoProvider.h"

#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

namespace uavlog {

DatabaseManager::DatabaseManager(const QString& connectionName)
    : m_connectionName(connectionName) {}

DatabaseManager::~DatabaseManager() {
    closeCleanly();
}

void DatabaseManager::closeCleanly() {
    if (m_db.isOpen()) {
        if (!m_dbPath.isEmpty() && m_dbPath != ":memory:") {
            // Сброс WAL при корректном закрытии приложения (ADR-0002)
            QSqlQuery cp(m_db);
            cp.exec("PRAGMA wal_checkpoint(TRUNCATE);");
        }
        m_db.close();
    }
    m_db = QSqlDatabase();
    if (QSqlDatabase::contains(m_connectionName)) {
        QSqlDatabase::removeDatabase(m_connectionName);
    }
}

bool DatabaseManager::openAndInitialize(const QString& dbFilePath, QString* errorOut) {
    closeCleanly();
    m_dbPath = dbFilePath;
    if (dbFilePath != ":memory:") {
        QDir().mkpath(QFileInfo(dbFilePath).absolutePath());
    }

    m_db = QSqlDatabase::addDatabase("QSQLITE", m_connectionName);
    m_db.setDatabaseName(dbFilePath);

    if (!m_db.open()) {
        if (errorOut) *errorOut = m_db.lastError().text();
        return false;
    }

    QSqlQuery pragma(m_db);
    pragma.exec("PRAGMA foreign_keys = ON;");
    pragma.exec("PRAGMA busy_timeout = 5000;");
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
        if (errorOut) *errorOut = "Ошибка выполнения PRAGMA integrity_check: " + q.lastError().text();
        return false;
    }
    const QString result = q.value(0).toString();
    if (result.compare("ok", Qt::CaseInsensitive) != 0) {
        if (errorOut) *errorOut = "Повреждение целостности БД: " + result;
        return false;
    }
    return true;
}

bool DatabaseManager::verifyExternalDbIntegrity(const QString& filePath, QString* errorOut) {
    if (!QFileInfo::exists(filePath)) {
        if (errorOut) *errorOut = "Файл базы данных не найден: " + filePath;
        return false;
    }
    const QString connName = "uavlog_verify_" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    bool ok = false;
    {
        QSqlDatabase checkDb = QSqlDatabase::addDatabase("QSQLITE", connName);
        checkDb.setDatabaseName(filePath);
        if (!checkDb.open()) {
            if (errorOut) *errorOut = "Не удалось открыть файл копии: " + checkDb.lastError().text();
        } else {
            QSqlQuery q(checkDb);
            if (!q.exec("PRAGMA integrity_check;") || !q.next()) {
                if (errorOut) *errorOut = "Файл копии повреждён или не является БД SQLite";
            } else if (q.value(0).toString().compare("ok", Qt::CaseInsensitive) != 0) {
                if (errorOut) *errorOut = "Проверка integrity_check не пройдена: " + q.value(0).toString();
            } else {
                // Дополнительно проверим наличие ключевой таблицы uavs
                QSqlQuery qt(checkDb);
                if (qt.exec("SELECT name FROM sqlite_master WHERE type='table' AND name='uavs';") && qt.next()) {
                    ok = true;
                } else {
                    if (errorOut) *errorOut = "Выбранный файл SQLite не содержит схему UAVLog";
                }
            }
            checkDb.close();
        }
    }
    QSqlDatabase::removeDatabase(connName);
    return ok;
}

bool DatabaseManager::createBackupVacuumInto(const QString& targetFilePath, QString* errorOut) {
    QDir().mkpath(QFileInfo(targetFilePath).absolutePath());
    if (QFile::exists(targetFilePath)) {
        QFile::remove(targetFilePath);
    }
    QString escaped = targetFilePath;
    escaped.replace('\'', "''");
    QSqlQuery q(m_db);
    if (!q.exec(QString("VACUUM INTO '%1';").arg(escaped))) {
        if (errorOut) *errorOut = q.lastError().text();
        return false;
    }
    return true;
}

bool DatabaseManager::restoreFromBackup(const QString& backupFilePath,
                                        const QString& safetyBackupDir,
                                        QString* safetyBackupPathOut,
                                        QString* errorOut) {
    if (m_dbPath.isEmpty() || m_dbPath == ":memory:") {
        if (errorOut) *errorOut = "Восстановление недоступно для БД в памяти";
        return false;
    }

    // 1. Проверяем валидность резервной копии до замены рабочей БД
    if (!verifyExternalDbIntegrity(backupFilePath, errorOut)) {
        return false;
    }

    // 2. Перед восстановлением создаём страховую копию текущей БД (REQ-FUNC-015)
    QDir().mkpath(safetyBackupDir);
    const QString safetyPath = QDir(safetyBackupDir).filePath(
        QString("uavlog_pre_restore_%1.db")
            .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss")));

    if (m_db.isOpen()) {
        QString vacErr;
        if (!createBackupVacuumInto(safetyPath, &vacErr)) {
            // Если текущая БД повреждена и VACUUM INTO не сработал — копируем сырой файл
            QFile::copy(m_dbPath, safetyPath);
        }
    } else if (QFile::exists(m_dbPath)) {
        QFile::copy(m_dbPath, safetyPath);
    }
    if (safetyBackupPathOut) *safetyBackupPathOut = safetyPath;

    // 3. Закрываем текущее соединение и удаляем служебные WAL/SHM файлы
    const QString targetPath = m_dbPath;
    closeCleanly();
    QFile::remove(targetPath + "-wal");
    QFile::remove(targetPath + "-shm");
    QFile::remove(targetPath);

    // 4. Копируем проверенную резервную копию на место рабочей БД
    if (!QFile::copy(backupFilePath, targetPath)) {
        if (errorOut) *errorOut = "Не удалось скопировать файл резервной копии в " + targetPath;
        return false;
    }

    // 5. Переоткрываем и проверяем целостность
    return openAndInitialize(targetPath, errorOut);
}

bool DatabaseManager::beginImmediate(std::string* errorOut) {
    QString qerr;
    const bool ok = beginImmediateQ(errorOut ? &qerr : nullptr);
    if (!ok && errorOut) *errorOut = qerr.toStdString();
    return ok;
}

bool DatabaseManager::commit(std::string* errorOut) {
    QString qerr;
    const bool ok = commitQ(errorOut ? &qerr : nullptr);
    if (!ok && errorOut) *errorOut = qerr.toStdString();
    return ok;
}

bool DatabaseManager::beginImmediateQ(QString* errorOut) {
    QSqlQuery q(m_db);
    if (!q.exec("BEGIN IMMEDIATE;")) {
        if (errorOut) *errorOut = q.lastError().text();
        return false;
    }
    return true;
}

bool DatabaseManager::commitQ(QString* errorOut) {
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
    // 16 таблиц + 5 составных индексов в точном соответствии с ER-схемой SDD DV-04 (v0.3+)
    const QStringList statements = {
        R"(CREATE TABLE IF NOT EXISTS users (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            login TEXT NOT NULL UNIQUE,
            full_name TEXT NOT NULL,
            password_hash TEXT NOT NULL,
            salt TEXT NOT NULL,
            iterations INTEGER NOT NULL DEFAULT 100000,
            role TEXT NOT NULL CHECK(role IN ('Administrator','Technician','Operator','Observer')),
            failed_attempts INTEGER NOT NULL DEFAULT 0,
            locked_until TEXT,
            active INTEGER NOT NULL DEFAULT 1
        );)",

        R"(CREATE TABLE IF NOT EXISTS audit_log (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id INTEGER REFERENCES users(id),
            created_at TEXT NOT NULL,
            entity_type TEXT NOT NULL,
            entity_id INTEGER,
            action TEXT NOT NULL,
            old_json TEXT,
            new_json TEXT
        );)",

        R"(CREATE TABLE IF NOT EXISTS uav_types (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL UNIQUE,
            manufacturer TEXT,
            default_payload TEXT,
            notes TEXT
        );)",

        R"(CREATE TABLE IF NOT EXISTS maintenance_rules (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            uav_type_id INTEGER NOT NULL REFERENCES uav_types(id) ON DELETE CASCADE,
            kind_name TEXT NOT NULL,
            interval_hours REAL,
            interval_days INTEGER,
            tasks_json TEXT NOT NULL,
            CHECK((interval_hours IS NOT NULL AND interval_hours > 0) OR (interval_days IS NOT NULL AND interval_days > 0))
        );)",

        R"(CREATE TABLE IF NOT EXISTS uavs (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            tail_number TEXT NOT NULL UNIQUE,
            uav_type_id INTEGER NOT NULL REFERENCES uav_types(id),
            serial_number TEXT NOT NULL,
            commissioned_date TEXT NOT NULL,
            state TEXT NOT NULL DEFAULT 'Serviceable'
                CHECK(state IN ('Serviceable','DueMaintenance','InMaintenance','Faulty','Retired')),
            total_flight_min INTEGER NOT NULL DEFAULT 0,
            total_flights INTEGER NOT NULL DEFAULT 0,
            last_flight_date TEXT,
            standard_payload TEXT,
            notes TEXT
        );)",

        R"(CREATE TABLE IF NOT EXISTS batteries (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            inventory_no TEXT NOT NULL UNIQUE,
            model_name TEXT NOT NULL,
            max_cycles INTEGER NOT NULL CHECK(max_cycles > 0),
            used_cycles INTEGER NOT NULL DEFAULT 0,
            assigned_uav_id INTEGER REFERENCES uavs(id) ON DELETE SET NULL,
            commissioned_date TEXT NOT NULL,
            retired INTEGER NOT NULL DEFAULT 0
        );)",

        R"(CREATE TABLE IF NOT EXISTS operators (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            full_name TEXT NOT NULL,
            position TEXT NOT NULL,
            permit_number TEXT NOT NULL,
            permit_expiry TEXT NOT NULL,
            active INTEGER NOT NULL DEFAULT 1
        );)",

        R"(CREATE TABLE IF NOT EXISTS operator_type_permits (
            operator_id INTEGER NOT NULL REFERENCES operators(id) ON DELETE CASCADE,
            uav_type_id INTEGER NOT NULL REFERENCES uav_types(id) ON DELETE CASCADE,
            granted_date TEXT NOT NULL,
            PRIMARY KEY (operator_id, uav_type_id)
        );)",

        R"(CREATE TABLE IF NOT EXISTS checklist_templates (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            uav_type_id INTEGER NOT NULL REFERENCES uav_types(id) ON DELETE CASCADE,
            name TEXT NOT NULL,
            active INTEGER NOT NULL DEFAULT 1,
            created_at TEXT NOT NULL
        );)",

        R"(CREATE TABLE IF NOT EXISTS checklist_items (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            template_id INTEGER NOT NULL REFERENCES checklist_templates(id) ON DELETE CASCADE,
            item_text TEXT NOT NULL,
            is_critical INTEGER NOT NULL DEFAULT 1,
            sort_order INTEGER NOT NULL DEFAULT 0
        );)",

        R"(CREATE TABLE IF NOT EXISTS preflight_checks (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            uav_id INTEGER NOT NULL REFERENCES uavs(id),
            template_id INTEGER NOT NULL REFERENCES checklist_templates(id),
            inspector_id INTEGER REFERENCES users(id),
            check_dt TEXT NOT NULL,
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

        R"(CREATE TABLE IF NOT EXISTS flights (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            uav_id INTEGER NOT NULL REFERENCES uavs(id),
            operator_id INTEGER NOT NULL REFERENCES operators(id),
            battery_id INTEGER NOT NULL REFERENCES batteries(id),
            preflight_id INTEGER REFERENCES preflight_checks(id),
            flight_date TEXT NOT NULL,
            takeoff TEXT NOT NULL,
            landing TEXT NOT NULL,
            duration_min INTEGER NOT NULL CHECK(duration_min > 0 AND duration_min <= 720),
            task_and_area TEXT,
            notes TEXT,
            created_by INTEGER REFERENCES users(id)
        );)",

        R"(CREATE TABLE IF NOT EXISTS maintenance_records (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            uav_id INTEGER NOT NULL REFERENCES uavs(id),
            rule_id INTEGER NOT NULL REFERENCES maintenance_rules(id),
            performed_by INTEGER REFERENCES users(id),
            performer_name TEXT NOT NULL,
            performed_date TEXT NOT NULL,
            flight_min_at_maint INTEGER NOT NULL,
            checklist_results_json TEXT NOT NULL,
            notes TEXT
        );)",

        R"(CREATE TABLE IF NOT EXISTS faults (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            uav_id INTEGER NOT NULL REFERENCES uavs(id),
            reported_by INTEGER REFERENCES users(id),
            reported_date TEXT NOT NULL,
            description TEXT NOT NULL,
            is_critical INTEGER NOT NULL DEFAULT 0,
            resolved_date TEXT,
            resolution_works TEXT
        );)",

        R"(CREATE TABLE IF NOT EXISTS app_settings (
            key TEXT PRIMARY KEY,
            value TEXT NOT NULL
        );)",

        // Пять составных индексов из SDD DV-04 (REQ-PERF-001..003)
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

    QSqlQuery tx(m_db);
    tx.exec("BEGIN IMMEDIATE;");

    auto insertUser = [&](const QString& login, const QString& full, const QString& role, const QString& pass) {
        const QString salt = CryptoProvider::generateSaltHex();
        const QString hash = CryptoProvider::hashPasswordPbkdf2(pass, salt);
        QSqlQuery q(m_db);
        q.prepare(R"(
            INSERT INTO users (login, full_name, password_hash, salt, iterations, role)
            VALUES (:l, :f, :h, :s, :it, :r)
        )");
        q.bindValue(":l", login);
        q.bindValue(":f", full);
        q.bindValue(":h", hash);
        q.bindValue(":s", salt);
        q.bindValue(":it", CryptoProvider::kDefaultIterations);
        q.bindValue(":r", role);
        q.exec();
    };

    // 4 роли пользователей из SRS 2.4
    insertUser("admin",    "Мороз А. А. (Нач. службы)",  "Administrator", "Admin1234!");
    insertUser("tech",     "Ковалёв И. В. (Техник ТО)",  "Technician",    "Tech1234!");
    insertUser("operator", "Соколов Д. С. (Оператор)",   "Operator",      "Oper1234!");
    insertUser("head",     "Волков П. Н. (Руководитель)","Observer",      "Head1234!");

    QSqlQuery q(m_db);

    // 2 типа БЛА + по 5 регламентов ТО на каждый тип (REQ-FUNC-002: не менее 5 видов ТО на тип)
    q.exec("INSERT INTO uav_types (id, name, manufacturer, default_payload, notes) VALUES "
           "(1, 'Квадрокоптер М-30', 'КБ Беспилотные системы', 'ОЭ-камера 4K / тепловизор', 'Тактический разведывательный квадрокоптер'),"
           "(2, 'Крыло С-10', 'ОАО Авиаприбор', 'Фотограмметрия 42 Мп', 'БЛА самолётного типа дальнего радиуса');");

    q.exec(R"(INSERT INTO maintenance_rules (id, uav_type_id, kind_name, interval_hours, interval_days, tasks_json) VALUES
        (1, 1, 'ТО-10 (оперативное)',      10.0, 14,  '["Осмотр лопастей и винтов","Проверка крепежа лучей","Чистка оптики камеры"]'),
        (2, 1, 'ТО-25 (регламентное)',     25.0, 30,  '["Проверка люфта электродвигателей","Калибровка IMU и компаса","Тест канала телеметрии","Проверка разъёмов питания"]'),
        (3, 1, 'ТО-50 (расширенное)',      50.0, 90,  '["Замена вибродемпферов подвеса","Диагностика регуляторов ESC","Проверка герметичности корпуса"]'),
        (4, 1, 'ТО-100 (капитальное)',    100.0, 180, '["Замена подшипников моторов","Полная ревизия проводки","Обновление прошивки контроллера"]'),
        (5, 1, 'ТО-Сезонное (календарь)', NULL,  180, '["Сезонная консервация/расконсервация","Проверка уплотнителей при перепадах температур"]'),
        (6, 2, 'ТО-15 (оперативное)',      15.0, 21,  '["Осмотр консолей крыла и рулей","Проверка тяг сервоприводов","Проверка замка парашюта"]'),
        (7, 2, 'ТО-50 (планер и СУ)',      50.0, 60,  '["Ревизия силовой установки","Проверка трубки Пито и датчика скорости","Калибровка автопилота"]'),
        (8, 2, 'ТО-100 (углублённое)',    100.0, 120, '["Переукладка спасательного парашюта","Замена сервоприводов элеронов","Дефектоскопия лонжеронов"]'),
        (9, 2, 'ТО-200 (капитальное)',    200.0, 240, '["Замена маршевого двигателя","Замена жгутов бортовой сети","Контрольный облёт"]'),
        (10,2, 'ТО-Годовое (календарь)',  NULL,  365, '["Ежегодная поверка датчиков","Проверка формуляра и ресурса планера"]');)");

    const QDate todayD = QDate::currentDate();
    const QString today     = todayD.toString(Qt::ISODate);
    const QString dAgo2     = todayD.addDays(-2).toString(Qt::ISODate);
    const QString dAgo5     = todayD.addDays(-5).toString(Qt::ISODate);
    const QString dAgo10    = todayD.addDays(-10).toString(Qt::ISODate);
    const QString dAgo12    = todayD.addDays(-12).toString(Qt::ISODate);
    const QString dAgo20    = todayD.addDays(-20).toString(Qt::ISODate);
    const QString dAgo30    = todayD.addDays(-30).toString(Qt::ISODate);
    const QString dAgo40    = todayD.addDays(-40).toString(Qt::ISODate);
    const QString dAgo70    = todayD.addDays(-70).toString(Qt::ISODate);
    const QString in90Days  = todayD.addDays(90).toString(Qt::ISODate);
    const QString in10Days  = todayD.addDays(10).toString(Qt::ISODate);
    const QString expired5  = todayD.addDays(-5).toString(Qt::ISODate);

    // 4 борта БЛА (состояния: Serviceable (в норме и скоро ТО), Serviceable (осмотр не пройден), DueMaintenance, Faulty)
    q.exec(QString(R"(INSERT INTO uavs (id, tail_number, uav_type_id, serial_number, commissioned_date, state, total_flight_min, total_flights, last_flight_date, standard_payload, notes) VALUES
        (1, 'БЛА-01', 1, 'SN-M30-001', '%1', 'Serviceable',    560, 6, '%2', 'ОЭ-камера 4K / тепловизор', 'Основной борт 1-го расчёта'),
        (2, 'БЛА-02', 1, 'SN-M30-002', '%1', 'Serviceable',    240, 4, '%3', 'ОЭ-камера 4K',              'Резервный борт 1-го расчёта'),
        (3, 'БЛА-03', 2, 'SN-S10-007', '%4', 'DueMaintenance', 960, 5, '%5', 'Фотограмметрия 42 Мп',      'Требуется плановое ТО-15'),
        (4, 'БЛА-04', 2, 'SN-S10-009', '%4', 'Faulty',         300, 2, '%5', 'Ретранслятор связи',        'Ожидает замены сервопривода');)")
               .arg(dAgo20, dAgo2, dAgo5, dAgo70, dAgo12));

    // АКБ: норма (4/200), порог >= 90 % (18/20), выработан 100 % (5/5 — не отображается в FlightDialog), свободная (1/200)
    // Заметим: реальные циклы пересчитываются по таблице flights — ниже для демо заведём согласованные полёты и базовый ресурс
    q.exec(QString(R"(INSERT INTO batteries (id, inventory_no, model_name, max_cycles, used_cycles, assigned_uav_id, commissioned_date, retired) VALUES
        (1, 'АКБ-101', 'LiPo 6S 5880mAh',  200, 6,  1, '%1', 0),
        (2, 'АКБ-102', 'LiPo 6S 5880mAh',  10,  9,  2, '%1', 0),
        (3, 'АКБ-103', 'LiPo 6S 5880mAh',  2,   2,  NULL, '%2', 0),
        (4, 'АКБ-201', 'Li-Ion 6S 12000mAh',150, 5,  3, '%2', 0);)")
               .arg(dAgo30, dAgo70));

    // 3 оператора: действующий (90 дн.), истекает (10 дн. <= 14 дн.), истёк (-5 дн.)
    q.exec(QString(R"(INSERT INTO operators (id, full_name, position, permit_number, permit_expiry, active) VALUES
        (1, 'Соколов Д. С.', 'Старший оператор БЛА', 'ДОП-2026-01', '%1', 1),
        (2, 'Петров А. И.',  'Оператор БЛА',         'ДОП-2026-04', '%2', 1),
        (3, 'Громов В. К.',  'Оператор БЛА',         'ДОП-2025-19', '%3', 1);)")
               .arg(in90Days, in10Days, expired5));

    q.exec(QString("INSERT INTO operator_type_permits (operator_id, uav_type_id, granted_date) VALUES "
                   "(1, 1, '%1'), (1, 2, '%1'), (2, 1, '%1'), (3, 1, '%1');").arg(dAgo70));

    // Шаблоны контрольных карт предполётного осмотра (F-15) для обоих типов БЛА
    q.exec(QString(R"(INSERT INTO checklist_templates (id, uav_type_id, name, active, created_at) VALUES
        (1, 1, 'Контрольная карта предполётного осмотра М-30', 1, '%1'),
        (2, 2, 'Контрольная карта предполётного осмотра С-10', 1, '%1');)").arg(dAgo30));

    q.exec(R"(INSERT INTO checklist_items (id, template_id, item_text, is_critical, sort_order) VALUES
        (1, 1, 'Целостность лопастей и фиксация воздушных винтов', 1, 1),
        (2, 1, 'Надёжность фиксации АКБ и уровень заряда не менее 90 %', 1, 2),
        (3, 1, 'Отсутствие люфта электродвигателей и креплений лучей', 1, 3),
        (4, 1, 'Стабильный приём GNSS (>= 12 спутников) и канала телеметрии', 1, 4),
        (5, 1, 'Чистота объектива полезной нагрузки и наличие карты памяти', 0, 5),
        (6, 2, 'Целостность консолей крыла, фюзеляжа и хвостового оперения', 1, 1),
        (7, 2, 'Свободный ход и отработка элеронов и руля высоты', 1, 2),
        (8, 2, 'Укладка парашюта и исправность пиропатрона/замка', 1, 3),
        (9, 2, 'Заряд бортовой АКБ и тест канала управления', 1, 4);)");

    // Предполётные осмотры на сегодня: БЛА-01 пройден, БЛА-02 с критичным замечанием
    q.exec(QString(R"(INSERT INTO preflight_checks (id, uav_id, template_id, inspector_id, check_dt, passed, notes) VALUES
        (1, 1, 1, 2, '%1', 1, 'Осмотр выполнен, замечаний нет — готов к вылету'),
        (2, 2, 1, 2, '%1', 0, 'Выявлен треснувший винт №2 (критичное замечание)');)").arg(today));

    q.exec(R"(INSERT INTO preflight_results (check_id, item_id, is_ok, comment) VALUES
        (1, 1, 1, ''), (1, 2, 1, ''), (1, 3, 1, ''), (1, 4, 1, ''), (1, 5, 1, ''),
        (2, 1, 0, 'Трещина на кромке винта №2'), (2, 2, 1, ''), (2, 3, 1, ''), (2, 4, 1, ''), (2, 5, 1, '');)");

    // Реальная история полётов (математически точно совпадает с uavs.total_flight_min и batteries.used_cycles!)
    // БЛА-01: 6 полётов с АКБ-101 = 560 мин (9.33 ч; ТО-10 на 10 ч -> остаток 0.67 ч <= 10% -> предупреждение!)
    // БЛА-02: 4 полёта с АКБ-102 = 240 мин + еще 5 исторических на БЛА-01/БЛА-02 -> чтобы циклы АКБ-102 были ровно 9 (90% из 10)
    // БЛА-03: 5 полётов с АКБ-201 = 960 мин (16.0 ч > ТО-15 = 15 ч -> просрочено ТО!)
    // БЛА-04: 2 полёта с АКБ-103 = 300 мин (циклы АКБ-103 = 2/2 = 100% -> выработан ресурс!)
    q.exec(QString(R"(INSERT INTO flights (uav_id, operator_id, battery_id, preflight_id, flight_date, takeoff, landing, duration_min, task_and_area, notes, created_by) VALUES
        (1, 1, 1, 1, '%1', '09:00', '10:30', 90,  'Воздушная разведка, сектор Север-1', 'Штатно', 3),
        (1, 1, 1, 1, '%1', '11:00', '12:40', 100, 'Мониторинг периметра, сектор Север-2', 'Штатно', 3),
        (1, 2, 1, 1, '%2', '08:30', '10:00', 90,  'Контрольный облёт, полигон', 'Ветер 6 м/с', 3),
        (1, 1, 1, 1, '%2', '13:00', '14:30', 90,  'Тепловизионный осмотр ЛЭП', 'Штатно', 3),
        (1, 2, 1, 1, '%3', '09:15', '10:55', 100, 'Учебный вылет, зона А', 'Штатно', 3),
        (1, 1, 1, 1, '%3', '14:00', '15:30', 90,  'Патрулирование, зона Б', 'Штатно', 3),
        (2, 2, 2, NULL, '%2', '10:00', '11:00', 60, 'Осмотр рубежа, сектор Восток', 'Штатно', 3),
        (2, 2, 2, NULL, '%2', '11:30', '12:30', 60, 'Осмотр рубежа, сектор Восток', 'Штатно', 3),
        (2, 1, 2, NULL, '%3', '09:00', '10:00', 60, 'Фотофиксация площадки', 'Штатно', 3),
        (2, 1, 2, NULL, '%3', '10:30', '11:30', 60, 'Проверка связи', 'Штатно', 3),
        (2, 1, 2, NULL, '%4', '09:00', '09:30', 30, 'Калибровочный облёт', 'Штатно', 3),
        (2, 1, 2, NULL, '%4', '10:00', '10:30', 30, 'Тренировка посадки', 'Штатно', 3),
        (2, 2, 2, NULL, '%4', '11:00', '11:30', 30, 'Тренировка висения', 'Штатно', 3),
        (2, 2, 2, NULL, '%4', '12:00', '12:30', 30, 'Проверка подвеса', 'Штатно', 3),
        (2, 2, 2, NULL, '%4', '13:00', '13:30', 30, 'Тест возврата домой', 'Штатно', 3),
        (3, 1, 4, NULL, '%4', '08:00', '11:20', 200, 'Картографирование, маршрут М-1', 'Штатно', 3),
        (3, 1, 4, NULL, '%4', '12:00', '15:20', 200, 'Картографирование, маршрут М-2', 'Штатно', 3),
        (3, 1, 4, NULL, '%5', '08:00', '11:10', 190, 'Аэрофотосъёмка, квадрат 14', 'Штатно', 3),
        (3, 1, 4, NULL, '%5', '12:00', '15:10', 190, 'Аэрофотосъёмка, квадрат 15', 'Штатно', 3),
        (3, 1, 4, NULL, '%6', '09:00', '12:00', 180, 'Дальний пролёт, маршрут М-3', 'Штатно', 3),
        (4, 1, 3, NULL, '%5', '09:00', '11:30', 150, 'Ретрансляция связи, точка Р-1', 'Помехи на посадке', 3),
        (4, 1, 3, NULL, '%6', '10:00', '12:30', 150, 'Ретрансляция связи, точка Р-2', 'Вибрация правого элерона', 3);)")
               .arg(dAgo2, dAgo5, dAgo10, dAgo12, dAgo30, dAgo40));

    // Пересинхронизируем кешированные агрегаты uavs и batteries точно по таблице flights!
    q.exec(R"(UPDATE uavs SET
        total_flight_min = COALESCE((SELECT SUM(duration_min) FROM flights WHERE flights.uav_id = uavs.id), 0),
        total_flights    = (SELECT COUNT(*) FROM flights WHERE flights.uav_id = uavs.id),
        last_flight_date = (SELECT MAX(flight_date) FROM flights WHERE flights.uav_id = uavs.id);)");
    q.exec("UPDATE batteries SET used_cycles = (SELECT COUNT(*) FROM flights WHERE flights.battery_id = batteries.id);");

    // Выполненное ТО (для БЛА-01 — ТО-10 выполнено 10 дней назад на налёте 0 мин, чтобы календарь был в норме, а налёт 560 мин давал 0.67 ч до ТО-10!)
    q.exec(QString(R"(INSERT INTO maintenance_records (uav_id, rule_id, performed_by, performer_name, performed_date, flight_min_at_maint, checklist_results_json, notes) VALUES
        (1, 1, 2, 'Ковалёв И. В.', '%1', 0, '[{"task":"Осмотр лопастей и винтов","done":true},{"task":"Проверка крепежа лучей","done":true},{"task":"Чистка оптики камеры","done":true}]', 'Плановое оперативное ТО перед циклом вылетов'),
        (2, 1, 2, 'Ковалёв И. В.', '%2', 150, '[{"task":"Осмотр лопастей и винтов","done":true},{"task":"Проверка крепежа лучей","done":true},{"task":"Чистка оптики камеры","done":true}]', 'Замечаний нет');)")
               .arg(dAgo10, dAgo5));

    // Неисправности (одна открытая критичная на БЛА-04 и одна устранённая на БЛА-01)
    q.exec(QString(R"(INSERT INTO faults (uav_id, reported_by, reported_date, description, is_critical, resolved_date, resolution_works) VALUES
        (4, 3, '%1', 'Люфт и подклинивание сервопривода правого элерона', 1, NULL, NULL),
        (1, 3, '%2', 'Потёртость изоляции разъёма телеметрии', 0, '%3', 'Установлена термоусадочная трубка, проверка канала пройдена');)")
               .arg(dAgo12, dAgo10, dAgo5));

    // Настраиваемые пороги предупреждений (REQ-FUNC-009-2, REQ-REUSE-001)
    q.exec("INSERT OR IGNORE INTO app_settings (key, value) VALUES "
           "('warn_maint_pct', '10'), ('warn_maint_days', '7'), "
           "('warn_permit_days', '14'), ('warn_battery_pct', '90');");

    // Стартовая запись аудита
    AuditLogger::writeAuditRecord(m_db, 1, "SYSTEM_INIT", "database", 1, "",
                                  "Инициализация БД UAVLog 1.0 (эталонные данные)");

    tx.exec("COMMIT;");
}

} // namespace uavlog
