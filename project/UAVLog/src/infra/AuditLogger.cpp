#include "infra/AuditLogger.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QTextStream>

namespace uavlog {

AuditLogger::AuditLogger(const QString& logDir, qint64 maxLogFileBytes)
    : m_logDir(logDir), m_maxLogFileBytes(maxLogFileBytes) {}

void AuditLogger::rotateIfNeeded(const QString& filePath) const {
    QFileInfo info(filePath);
    if (!info.exists() || info.size() < m_maxLogFileBytes) {
        return;
    }

    // Удаляем самый старый (uavlog.log.5) и сдвигаем 4->5, ..., main->1
    for (int i = kMaxLogFiles; i >= 1; --i) {
        const QString src = (i == 1) ? filePath : QString("%1.%2").arg(filePath).arg(i - 1);
        const QString dst = QString("%1.%2").arg(filePath).arg(i);
        if (QFile::exists(dst)) {
            QFile::remove(dst);
        }
        if (QFile::exists(src)) {
            QFile::rename(src, dst);
        }
    }
}

void AuditLogger::logError(const QString& level,
                           const QString& context,
                           const QString& message) const {
    if (m_logDir.isEmpty()) return;
    QDir().mkpath(m_logDir);
    const QString filePath = QDir(m_logDir).filePath("uavlog.log");
    rotateIfNeeded(filePath);

    QFile file(filePath);
    if (file.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&file);
        out << QDateTime::currentDateTime().toString(Qt::ISODate)
            << " [" << level << "] [" << context << "] " << message << "\n";
    }
}

bool AuditLogger::writeAuditRecord(QSqlDatabase& db,
                                   int userId,
                                   const QString& action,
                                   const QString& entityType,
                                   int entityId,
                                   const QString& oldJson,
                                   const QString& newJson,
                                   QString* errorOut) {
    QSqlQuery q(db);
    q.prepare(R"(
        INSERT INTO audit_log (user_id, created_at, entity_type, entity_id, action, old_json, new_json)
        VALUES (:uid, :ts, :etype, :eid, :act, :oldj, :newj)
    )");
    q.bindValue(":uid", userId > 0 ? QVariant(userId) : QVariant());
    q.bindValue(":ts", QDateTime::currentDateTime().toString(Qt::ISODate));
    q.bindValue(":etype", entityType);
    q.bindValue(":eid", entityId);
    q.bindValue(":act", action);
    q.bindValue(":oldj", oldJson);
    q.bindValue(":newj", newJson);
    if (!q.exec()) {
        if (errorOut) *errorOut = q.lastError().text();
        return false;
    }
    return true;
}

} // namespace uavlog
