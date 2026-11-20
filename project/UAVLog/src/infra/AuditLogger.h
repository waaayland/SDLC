#pragma once

#include <QSqlDatabase>
#include <QString>

namespace uavlog {

class AuditLogger {
public:
    static constexpr qint64 kMaxLogFileBytes = 10LL * 1024 * 1024; // 10 МБ (REQ-OBS-001)
    static constexpr int kMaxLogFiles = 5;

    explicit AuditLogger(const QString& logDir = {});

    void logError(const QString& level, const QString& context, const QString& message) const;

    static bool writeAuditRecord(QSqlDatabase& db,
                                 int userId,
                                 const QString& action,
                                 const QString& entity,
                                 int entityId,
                                 const QString& oldValue,
                                 const QString& newValue,
                                 QString* errorOut = nullptr);

private:
    void rotateIfNeeded(const QString& filePath) const;
    QString m_logDir;
};

} // namespace uavlog
