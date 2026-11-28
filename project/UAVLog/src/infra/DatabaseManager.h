#pragma once

#include "core/Repositories.h"

#include <QSqlDatabase>
#include <QString>

namespace uavlog {

// Управление соединением SQLite 3 (WAL, foreign_keys=ON, synchronous=FULL) и Unit of Work (ADR-0002)
class DatabaseManager final : public IUnitOfWork {
public:
    explicit DatabaseManager(const QString& connectionName = "uavlog_main_conn");
    ~DatabaseManager() override;

    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

    bool openAndInitialize(const QString& dbFilePath, QString* errorOut = nullptr);
    void closeCleanly();
    bool checkIntegrity(QString* errorOut = nullptr);
    void seedDemoDataIfEmpty();

    // Горячее резервное копирование VACUUM INTO (REQ-FUNC-015, ADR-0002)
    bool createBackupVacuumInto(const QString& targetFilePath, QString* errorOut = nullptr);

    // Восстановление из резервной копии с проверкой целостности и страховой копией текущей БД
    bool restoreFromBackup(const QString& backupFilePath,
                           const QString& safetyBackupDir,
                           QString* safetyBackupPathOut = nullptr,
                           QString* errorOut = nullptr);

    // IUnitOfWork (чистый C++17 интерфейс)
    bool beginImmediate(std::string* errorOut = nullptr) override;
    bool commit(std::string* errorOut = nullptr) override;
    void rollback() noexcept override;

    // Qt-перегрузки для удобства сервисов
    bool beginImmediateQ(QString* errorOut = nullptr);
    bool commitQ(QString* errorOut = nullptr);

    QSqlDatabase& database() { return m_db; }
    const QString& dbPath() const { return m_dbPath; }

    static bool verifyExternalDbIntegrity(const QString& filePath, QString* errorOut = nullptr);

private:
    bool applySchema(QString* errorOut);

    QString m_connectionName;
    QString m_dbPath;
    QSqlDatabase m_db;
};

} // namespace uavlog
