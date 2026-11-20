#include "services/AuthService.h"
#include "infra/CryptoProvider.h"

#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>

namespace uavlog {

AuthService::AuthService(DatabaseManager& db, AuditLogger& audit)
    : m_db(db), m_audit(audit) {}

LoginOutcome AuthService::authenticate(const QString& username, const QString& password) {
    LoginOutcome out;
    const QString trimmedUser = username.trimmed();
    if (trimmedUser.isEmpty() || password.isEmpty()) {
        out.errorMessage = "Неверный логин или пароль";
        return out;
    }

    QSqlQuery q(m_db.database());
    q.prepare(R"(
        SELECT id, username, full_name, role, password_hash, salt, iterations,
               failed_attempts, locked_until, active
        FROM users WHERE username = :u
    )");
    q.bindValue(":u", trimmedUser);
    if (!q.exec() || !q.next()) {
        out.errorMessage = "Неверный логин или пароль";
        return out;
    }

    const int userId = q.value(0).toInt();
    const QString fullName = q.value(2).toString();
    const QString roleStr = q.value(3).toString();
    const QString hashHex = q.value(4).toString();
    const QString saltHex = q.value(5).toString();
    const int iterations = q.value(6).toInt();
    const int failedAttempts = q.value(7).toInt();
    const QString lockedUntilStr = q.value(8).toString();
    const bool active = q.value(9).toInt() != 0;

    if (!active) {
        out.errorMessage = "Учётная запись деактивирована";
        return out;
    }

    const QDateTime now = QDateTime::currentDateTime();
    if (!lockedUntilStr.isEmpty()) {
        const QDateTime lockedUntil = QDateTime::fromString(lockedUntilStr, Qt::ISODate);
        if (lockedUntil.isValid() && lockedUntil > now) {
            const qint64 secs = now.secsTo(lockedUntil);
            out.locked = true;
            out.errorMessage = QString("Учётная запись временно заблокирована (осталось %1 мин %2 с)")
                                   .arg(secs / 60)
                                   .arg(secs % 60);
            return out;
        }
    }

    if (!CryptoProvider::verifyPassword(password, saltHex, hashHex, iterations)) {
        const int nextAttempts = failedAttempts + 1;
        QSqlQuery upd(m_db.database());
        if (nextAttempts >= kMaxFailedAttempts) {
            const QString lockStr = now.addSecs(kLockoutMinutes * 60).toString(Qt::ISODate);
            upd.prepare("UPDATE users SET failed_attempts = :a, locked_until = :l WHERE id = :id");
            upd.bindValue(":a", nextAttempts);
            upd.bindValue(":l", lockStr);
            upd.bindValue(":id", userId);
            upd.exec();
            m_audit.logError("WARN", "AuthService", "Блокировка аккаунта после 5 неудачных попыток: " + trimmedUser);
            out.locked = true;
            out.errorMessage = "Превышено число попыток входа (5). Учётная запись заблокирована на 5 минут.";
        } else {
            upd.prepare("UPDATE users SET failed_attempts = :a WHERE id = :id");
            upd.bindValue(":a", nextAttempts);
            upd.bindValue(":id", userId);
            upd.exec();
            out.errorMessage = "Неверный логин или пароль";
        }
        return out;
    }

    // Успешный вход: сброс счётчика ошибок и запись в audit_log (UC-01 -> include UC-11)
    QSqlQuery reset(m_db.database());
    reset.prepare("UPDATE users SET failed_attempts = 0, locked_until = NULL WHERE id = :id");
    reset.bindValue(":id", userId);
    reset.exec();

    AuditLogger::writeAuditRecord(m_db.database(), userId, "LOGIN", "users", userId, "", trimmedUser);

    UserAccount acc;
    acc.id = userId;
    acc.username = trimmedUser.toStdString();
    acc.fullName = fullName.toStdString();
    acc.role = fromDbRole(roleStr.toStdString());
    acc.active = true;

    out.success = true;
    out.user = acc;
    return out;
}

} // namespace uavlog
