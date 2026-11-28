#pragma once

#include <QSqlDatabase>
#include <QString>

namespace uavlog {

// Сквозной аудит БД (REQ-SEC-004) и ротируемый журнал ошибок 5 x 10 МБ (REQ-OBS-001)
class AuditLogger {
public:
    static constexpr qint64 kDefaultMaxLogFileBytes = 10LL * 1024 * 1024;
    static constexpr int kMaxLogFiles = 5;

    explicit AuditLogger(const QString& logDir = {},
                         qint64 maxLogFileBytes = kDefaultMaxLogFileBytes);

    void logError(const QString& level, const QString& context, const QString& message) const;
    QString logDir() const { return m_logDir; }

    static bool writeAuditRecord(QSqlDatabase& db,
                                 int userId,
                                 const QString& action,
                                 const QString& entityType,
                                 int entityId,
                                 const QString& oldJson,
                                 const QString& newJson,
                                 QString* errorOut = nullptr);

private:
    void rotateIfNeeded(const QString& filePath) const;

    QString m_logDir;
    qint64 m_maxLogFileBytes = kDefaultMaxLogFileBytes;
};

} // namespace uavlog
