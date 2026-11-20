#pragma once

#include <QSqlDatabase>
#include <QString>

namespace uavlog {

class DatabaseManager {
public:
    static constexpr int kSchemaVersion = 1;

    explicit DatabaseManager(const QString& connectionName = QStringLiteral("uavlog_main"));
    ~DatabaseManager();

    bool openAndInitialize(const QString& dbFilePath, QString* errorOut = nullptr);
    bool checkIntegrity(QString* errorOut = nullptr);
    bool createBackupVacuumInto(const QString& targetFilePath, QString* errorOut = nullptr);
    void seedDemoDataIfEmpty();

    bool beginImmediate(QString* errorOut = nullptr);
    bool commit(QString* errorOut = nullptr);
    void rollback() noexcept;

    QSqlDatabase& database() { return m_db; }
    const QString& dbPath() const { return m_dbPath; }

private:
    bool applySchema(QString* errorOut);

    QString m_connectionName;
    QString m_dbPath;
    QSqlDatabase m_db;
};

} // namespace uavlog
