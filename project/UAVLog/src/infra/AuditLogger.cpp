#include "infra/AuditLogger.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QTextStream>

namespace uavlog {

AuditLogger::AuditLogger(const QString& logDir)
    : m_logDir(logDir) {}

void AuditLogger::rotateIfNeeded(const QString& filePath) const {
    QFileInfo info(filePath);
    if (!info.exists() || info.size() < kMaxLogFileBytes) {
        return;
    }
    for (int i = kMaxLogFiles - 1; i >= 1; --i) {
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
                                   const QString& entity,
                                   int entityId,
                                   const QString& oldValue,
                                   const QString& newValue,
                                   QString* errorOut) {
    QSqlQuery q(db);
    q.prepare(R"(
        INSERT INTO audit_log (user_id, created_at, action, entity, entity_id, old_value, new_value)
        VALUES (:uid, :ts, :act, :ent, :eid, :oldv, :newv)
    )");
    q.bindValue(":uid", userId);
    q.bindValue(":ts", QDateTime::currentDateTime().toString(Qt::ISODate));
    q.bindValue(":act", action);
    q.bindValue(":ent", entity);
    q.bindValue(":eid", entityId);
    q.bindValue(":oldv", oldValue);
    q.bindValue(":newv", newValue);
    if (!q.exec()) {
        if (errorOut) *errorOut = q.lastError().text();
        return false;
    }
    return true;
}

} // namespace uavlog
