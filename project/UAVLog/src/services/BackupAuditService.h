#pragma once

#include "core/DomainTypes.h"
#include "infra/AuditLogger.h"
#include "infra/DatabaseManager.h"

#include <QString>
#include <QStringList>
#include <vector>

namespace uavlog {

// Сервис аудита (F-13, REQ-SEC-004) и резервного копирования/восстановления БД (F-14, REQ-FUNC-015, REQ-REL-002)
class BackupAuditService {
public:
    static constexpr int kMaxAutoBackups = 30;      // REQ-REL-002
    static constexpr qint64 kAutoBackupHours = 24;

    BackupAuditService(DatabaseManager& db, AuditLogger& audit);

    // Журнал аудита (только чтение и CSV, доступ только Администратору)
    std::vector<AuditEntry> loadAuditEntries(const UserAccount& actor,
                                             const QString& searchFilter = {},
                                             QString* errorOut = nullptr);

    bool exportAuditCsv(const UserAccount& actor,
                        const QString& filePath,
                        const QString& searchFilter = {},
                        QString* errorOut = nullptr);

    // Ручное резервное копирование и восстановление (REQ-FUNC-015)
    bool createManualBackup(const UserAccount& actor,
                            const QString& targetFilePath,
                            QString* errorOut = nullptr);

    bool restoreFromBackup(const UserAccount& actor,
                           const QString& backupFilePath,
                           const QString& safetyBackupDir,
                           QString* safetyBackupPathOut = nullptr,
                           QString* errorOut = nullptr);

    // Автоматическое резервное копирование при выходе (раз в 24 ч, ротация 30 копий FIFO — REQ-REL-002)
    bool performAutoBackupIfNeeded(const QString& backupsDir,
                                   bool forceNow = false,
                                   QString* createdPathOut = nullptr,
                                   QString* errorOut = nullptr);

    static QStringList listBackupFiles(const QString& backupsDir);

private:
    static void rotateAutoBackups(const QString& backupsDir, int maxCopies = kMaxAutoBackups);

    DatabaseManager& m_db;
    AuditLogger& m_audit;
};

} // namespace uavlog
