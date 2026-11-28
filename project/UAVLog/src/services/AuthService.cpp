#include "services/AuthService.h"
#include "core/RbacGuard.h"
#include "infra/CryptoProvider.h"

#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>

namespace uavlog {

AuthService::AuthService(DatabaseManager& db, AuditLogger& audit)
    : m_db(db), m_audit(audit) {}

AuthResult AuthService::authenticate(const QString& login, const QString& password) {
    return authenticateInternal(login, password, true);
}

AuthResult AuthService::verifyCurrentUserPassword(const QString& login, const QString& password) {
    return authenticateInternal(login, password, false);
}

AuthResult AuthService::authenticateInternal(const QString& login,
                                             const QString& password,
                                             bool logAuditOnSuccess) {
    AuthResult res;
    const QString trimmed = login.trimmed();
    if (trimmed.isEmpty() || password.isEmpty()) {
        res.errorMessage = "Неверный логин или пароль";
        return res;
    }

    QSqlQuery q(m_db.database());
    q.prepare(R"(
        SELECT id, login, full_name, role, password_hash, salt, iterations,
               failed_attempts, locked_until, active
        FROM users WHERE login = :login
    )");
    q.bindValue(":login", trimmed);
    if (!q.exec() || !q.next()) {
        res.errorMessage = "Неверный логин или пароль";
        return res;
    }

    const int userId = q.value(0).toInt();
    const QString dbLogin = q.value(1).toString();
    const QString fullName = q.value(2).toString();
    const UserRole role = fromDbRole(q.value(3).toString().toStdString());
    const QString storedHash = q.value(4).toString();
    const QString salt = q.value(5).toString();
    const int iterations = q.value(6).toInt();
    const int failedAttempts = q.value(7).toInt();
    const QString lockedUntilStr = q.value(8).toString();
    const bool active = q.value(9).toInt() != 0;

    if (!active) {
        res.errorMessage = "Учётная запись отключена администратором";
        return res;
    }

    const QDateTime now = QDateTime::currentDateTimeUtc();
    if (!lockedUntilStr.isEmpty()) {
        const QDateTime lockedUntil = QDateTime::fromString(lockedUntilStr, Qt::ISODate);
        if (lockedUntil.isValid() && lockedUntil > now) {
            res.locked = true;
            res.lockSecondsRemaining = static_cast<int>(now.secsTo(lockedUntil));
            res.errorMessage = QString("Учётная запись заблокирована после 5 неудачных попыток. "
                                       "Повторите вход через %1 сек.")
                                   .arg(res.lockSecondsRemaining);
            return res;
        }
    }

    if (!CryptoProvider::verifyPassword(password, salt, storedHash, iterations)) {
        const int nextFailed = failedAttempts + 1;
        QSqlQuery upd(m_db.database());
        if (nextFailed >= kMaxFailedAttempts) {
            const QDateTime lockTime = now.addSecs(kLockoutMinutes * 60);
            upd.prepare("UPDATE users SET failed_attempts = :fa, locked_until = :lu WHERE id = :id;");
            upd.bindValue(":fa", nextFailed);
            upd.bindValue(":lu", lockTime.toString(Qt::ISODate));
            upd.bindValue(":id", userId);
            upd.exec();

            res.locked = true;
            res.lockSecondsRemaining = kLockoutMinutes * 60;
            res.errorMessage = "Превышено число попыток входа (5). Учётная запись заблокирована на 5 минут.";
            m_audit.logError("WARN", "AuthService", "Блокировка учётной записи на 5 минут: " + dbLogin);
        } else {
            upd.prepare("UPDATE users SET failed_attempts = :fa WHERE id = :id;");
            upd.bindValue(":fa", nextFailed);
            upd.bindValue(":id", userId);
            upd.exec();
            res.errorMessage = QString("Неверный логин или пароль (попытка %1 из %2)")
                                   .arg(nextFailed)
                                   .arg(kMaxFailedAttempts);
        }
        return res;
    }

    // Успешная аутентификация — сбрасываем счётчик ошибок и блокировку
    QSqlQuery reset(m_db.database());
    reset.prepare("UPDATE users SET failed_attempts = 0, locked_until = NULL WHERE id = :id;");
    reset.bindValue(":id", userId);
    reset.exec();

    if (logAuditOnSuccess) {
        AuditLogger::writeAuditRecord(m_db.database(), userId, "LOGIN", "users", userId, "",
                                      QString("login=%1;role=%2").arg(dbLogin, QString::fromStdString(toDbRole(role))));
    }

    UserAccount acc;
    acc.id = userId;
    acc.username = dbLogin.toStdString();
    acc.fullName = fullName.toStdString();
    acc.role = role;
    acc.active = active;

    res.success = true;
    res.user = std::move(acc);
    return res;
}

std::vector<UserAccount> AuthService::listUsers(const UserAccount& actor, QString* errorOut) {
    std::vector<UserAccount> users;
    try {
        RbacGuard::requirePermission(actor.role, Permission::ManageUsersAndSettings);
    } catch (const std::exception& ex) {
        if (errorOut) *errorOut = QString::fromUtf8(ex.what());
        return users;
    }

    QSqlQuery q(m_db.database());
    q.exec("SELECT id, login, full_name, role, active FROM users ORDER BY id;");
    while (q.next()) {
        UserAccount u;
        u.id = q.value(0).toInt();
        u.username = q.value(1).toString().toStdString();
        u.fullName = q.value(2).toString().toStdString();
        u.role = fromDbRole(q.value(3).toString().toStdString());
        u.active = q.value(4).toInt() != 0;
        users.push_back(std::move(u));
    }
    return users;
}

bool AuthService::createUser(const QString& login,
                             const QString& fullName,
                             UserRole role,
                             const QString& password,
                             const UserAccount& actor,
                             QString* errorOut) {
    try {
        RbacGuard::requirePermission(actor.role, Permission::ManageUsersAndSettings);
    } catch (const std::exception& ex) {
        if (errorOut) *errorOut = QString::fromUtf8(ex.what());
        return false;
    }

    const QString cleanLogin = login.trimmed();
    const QString cleanName = fullName.trimmed();
    if (cleanLogin.isEmpty() || cleanName.isEmpty()) {
        if (errorOut) *errorOut = "Логин и ФИО пользователя обязательны";
        return false;
    }
    if (!CryptoProvider::isPasswordPolicySatisfied(password)) {
        if (errorOut) *errorOut = "Пароль должен содержать не менее 8 символов (REQ-SEC-001)";
        return false;
    }

    const QString salt = CryptoProvider::generateSaltHex();
    const QString hash = CryptoProvider::hashPasswordPbkdf2(password, salt);

    if (!m_db.beginImmediateQ(errorOut)) return false;

    QSqlQuery q(m_db.database());
    q.prepare(R"(
        INSERT INTO users (login, full_name, password_hash, salt, iterations, role, active)
        VALUES (:l, :f, :h, :s, :it, :r, 1)
    )");
    q.bindValue(":l", cleanLogin);
    q.bindValue(":f", cleanName);
    q.bindValue(":h", hash);
    q.bindValue(":s", salt);
    q.bindValue(":it", CryptoProvider::kDefaultIterations);
    q.bindValue(":r", QString::fromStdString(toDbRole(role)));

    if (!q.exec()) {
        if (errorOut) *errorOut = "Не удалось создать пользователя (возможно, логин занят): " + q.lastError().text();
        m_db.rollback();
        return false;
    }
    const int newId = q.lastInsertId().toInt();
    AuditLogger::writeAuditRecord(m_db.database(), actor.id, "CREATE", "users", newId, "",
                                  QString("login=%1;role=%2").arg(cleanLogin, QString::fromStdString(toDbRole(role))));
    return m_db.commitQ(errorOut);
}

bool AuthService::setUserActive(int userId, bool active, const UserAccount& actor, QString* errorOut) {
    try {
        RbacGuard::requirePermission(actor.role, Permission::ManageUsersAndSettings);
    } catch (const std::exception& ex) {
        if (errorOut) *errorOut = QString::fromUtf8(ex.what());
        return false;
    }
    if (userId == actor.id && !active) {
        if (errorOut) *errorOut = "Нельзя отключить собственную учётную запись";
        return false;
    }
    if (!m_db.beginImmediateQ(errorOut)) return false;

    QSqlQuery q(m_db.database());
    q.prepare("UPDATE users SET active = :a WHERE id = :id;");
    q.bindValue(":a", active ? 1 : 0);
    q.bindValue(":id", userId);
    if (!q.exec()) {
        if (errorOut) *errorOut = q.lastError().text();
        m_db.rollback();
        return false;
    }
    AuditLogger::writeAuditRecord(m_db.database(), actor.id, "UPDATE", "users", userId, "",
                                  QString("active=%1").arg(active ? 1 : 0));
    return m_db.commitQ(errorOut);
}

bool AuthService::resetPassword(int userId, const QString& newPassword, const UserAccount& actor, QString* errorOut) {
    try {
        RbacGuard::requirePermission(actor.role, Permission::ManageUsersAndSettings);
    } catch (const std::exception& ex) {
        if (errorOut) *errorOut = QString::fromUtf8(ex.what());
        return false;
    }
    if (!CryptoProvider::isPasswordPolicySatisfied(newPassword)) {
        if (errorOut) *errorOut = "Пароль должен содержать не менее 8 символов (REQ-SEC-001)";
        return false;
    }

    const QString salt = CryptoProvider::generateSaltHex();
    const QString hash = CryptoProvider::hashPasswordPbkdf2(newPassword, salt);

    if (!m_db.beginImmediateQ(errorOut)) return false;

    QSqlQuery q(m_db.database());
    q.prepare("UPDATE users SET password_hash = :h, salt = :s, iterations = :it, failed_attempts = 0, locked_until = NULL WHERE id = :id;");
    q.bindValue(":h", hash);
    q.bindValue(":s", salt);
    q.bindValue(":it", CryptoProvider::kDefaultIterations);
    q.bindValue(":id", userId);
    if (!q.exec()) {
        if (errorOut) *errorOut = q.lastError().text();
        m_db.rollback();
        return false;
    }
    AuditLogger::writeAuditRecord(m_db.database(), actor.id, "PASSWORD_RESET", "users", userId, "", "password_updated");
    return m_db.commitQ(errorOut);
}

} // namespace uavlog
