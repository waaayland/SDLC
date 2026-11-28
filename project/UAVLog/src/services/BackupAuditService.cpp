#include "services/BackupAuditService.h"
#include "core/RbacGuard.h"
#include "infra/CsvExporter.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlQuery>

namespace uavlog {

BackupAuditService::BackupAuditService(DatabaseManager& db, AuditLogger& audit)
    : m_db(db), m_audit(audit) {}

std::vector<AuditEntry> BackupAuditService::loadAuditEntries(const UserAccount& actor,
                                                             const QString& searchFilter,
                                                             QString* errorOut) {
    std::vector<AuditEntry> list;
    try {
        RbacGuard::requirePermission(actor.role, Permission::ViewAuditLog);
    } catch (const std::exception& ex) {
        if (errorOut) *errorOut = QString::fromUtf8(ex.what());
        return list;
    }

    QString sql = R"(
        SELECT a.id, COALESCE(a.user_id, 0), COALESCE(u.login, 'system'), a.created_at,
               a.entity_type, COALESCE(a.entity_id, 0), a.action,
               COALESCE(a.old_json, ''), COALESCE(a.new_json, '')
        FROM audit_log a
        LEFT JOIN users u ON u.id = a.user_id
    )";
    const QString flt = searchFilter.trimmed();
    if (!flt.isEmpty()) {
        sql += " WHERE a.action LIKE :q OR a.entity_type LIKE :q OR u.login LIKE :q OR a.new_json LIKE :q";
    }
    sql += " ORDER BY a.id DESC LIMIT 2000;";

    QSqlQuery q(m_db.database());
    q.prepare(sql);
    if (!flt.isEmpty()) {
        q.bindValue(":q", "%" + flt + "%");
    }
    q.exec();
    while (q.next()) {
        AuditEntry e;
        e.id = q.value(0).toInt();
        e.userId = q.value(1).toInt();
        e.username = q.value(2).toString().toStdString();
        e.createdAtIso = q.value(3).toString().toStdString();
        e.entityType = q.value(4).toString().toStdString();
        e.entityId = q.value(5).toInt();
        e.action = q.value(6).toString().toStdString();
        e.oldValue = q.value(7).toString().toStdString();
        e.newValue = q.value(8).toString().toStdString();
        list.push_back(std::move(e));
    }
    return list;
}

bool BackupAuditService::exportAuditCsv(const UserAccount& actor,
                                        const QString& filePath,
                                        const QString& searchFilter,
                                        QString* errorOut) {
    const auto entries = loadAuditEntries(actor, searchFilter, errorOut);
    if (errorOut && !errorOut->isEmpty()) return false;

    const QStringList headers = {"ID", "Дата и время", "Пользователь", "Действие", "Объект", "ID объекта", "Было", "Стало"};
    std::vector<QStringList> rows;
    rows.reserve(entries.size());
    for (const auto& e : entries) {
        rows.push_back({
            QString::number(e.id),
            QString::fromStdString(e.createdAtIso),
            QString::fromStdString(e.username),
            QString::fromStdString(e.action),
            QString::fromStdString(e.entityType),
            QString::number(e.entityId),
            QString::fromStdString(e.oldValue),
            QString::fromStdString(e.newValue)
        });
    }
    return CsvExporter::exportTable(filePath, headers, rows, errorOut);
}

bool BackupAuditService::createManualBackup(const UserAccount& actor,
                                            const QString& targetFilePath,
                                            QString* errorOut) {
    try {
        RbacGuard::requirePermission(actor.role, Permission::ManageBackups);
    } catch (const std::exception& ex) {
        if (errorOut) *errorOut = QString::fromUtf8(ex.what());
        return false;
    }

    if (!m_db.createBackupVacuumInto(targetFilePath, errorOut)) {
        m_audit.logError("ERROR", "BackupAuditService::createManualBackup",
                         errorOut ? *errorOut : "VACUUM INTO failed");
        return false;
    }

    AuditLogger::writeAuditRecord(m_db.database(), actor.id, "BACKUP_MANUAL", "database", 0, "", targetFilePath);
    return true;
}

bool BackupAuditService::restoreFromBackup(const UserAccount& actor,
                                           const QString& backupFilePath,
                                           const QString& safetyBackupDir,
                                           QString* safetyBackupPathOut,
                                           QString* errorOut) {
    try {
        RbacGuard::requirePermission(actor.role, Permission::ManageBackups);
    } catch (const std::exception& ex) {
        if (errorOut) *errorOut = QString::fromUtf8(ex.what());
        return false;
    }

    if (!m_db.restoreFromBackup(backupFilePath, safetyBackupDir, safetyBackupPathOut, errorOut)) {
        m_audit.logError("ERROR", "BackupAuditService::restoreFromBackup",
                         errorOut ? *errorOut : "Restore failed");
        return false;
    }

    AuditLogger::writeAuditRecord(m_db.database(), actor.id, "BACKUP_RESTORE", "database", 0, "", backupFilePath);
    return true;
}

bool BackupAuditService::performAutoBackupIfNeeded(const QString& backupsDir,
                                                   bool forceNow,
                                                   QString* createdPathOut,
                                                   QString* errorOut) {
    QDir dir(backupsDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    const QFileInfoList existing = dir.entryInfoList(
        {"uavlog_auto_*.db"}, QDir::Files, QDir::Time); // Новейший первый

    if (!forceNow && !existing.isEmpty()) {
        const QDateTime lastModified = existing.first().lastModified();
        if (lastModified.secsTo(QDateTime::currentDateTime()) < kAutoBackupHours * 3600) {
            return true; // Копия младше 24 часов уже существует
        }
    }

    const QString fileName = QString("uavlog_auto_%1.db")
                                 .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz"));
    const QString targetPath = dir.filePath(fileName);
    if (!m_db.createBackupVacuumInto(targetPath, errorOut)) {
        m_audit.logError("ERROR", "BackupAuditService::performAutoBackupIfNeeded",
                         errorOut ? *errorOut : "Auto backup failed");
        return false;
    }
    if (createdPathOut) *createdPathOut = targetPath;

    rotateAutoBackups(backupsDir, kMaxAutoBackups);
    return true;
}

void BackupAuditService::rotateAutoBackups(const QString& backupsDir, int maxCopies) {
    QDir dir(backupsDir);
    const QFileInfoList files = dir.entryInfoList(
        {"uavlog_auto_*.db"}, QDir::Files, QDir::Name | QDir::Reversed); // Самые новые сначала по timestamp в имени
    for (int i = maxCopies; i < files.size(); ++i) {
        QFile::remove(files.at(i).absoluteFilePath());
    }
}

QStringList BackupAuditService::listBackupFiles(const QString& backupsDir) {
    QDir dir(backupsDir);
    if (!dir.exists()) return {};
    const QFileInfoList files = dir.entryInfoList({"*.db"}, QDir::Files, QDir::Time);
    QStringList out;
    for (const auto& fi : files) {
        out << fi.absoluteFilePath();
    }
    return out;
}

} // namespace uavlog
