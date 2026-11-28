#pragma once

#include "core/DomainTypes.h"
#include "infra/AuditLogger.h"
#include "infra/DatabaseManager.h"

#include <QString>
#include <optional>
#include <vector>

namespace uavlog {

struct AuthResult {
    bool success = false;
    bool locked = false;
    int lockSecondsRemaining = 0;
    QString errorMessage;
    std::optional<UserAccount> user;
};

class AuthService {
public:
    static constexpr int kMaxFailedAttempts = 5;    // REQ-SEC-005
    static constexpr int kLockoutMinutes = 5;

    AuthService(DatabaseManager& db, AuditLogger& audit);

    AuthResult authenticate(const QString& login, const QString& password);
    AuthResult verifyCurrentUserPassword(const QString& login, const QString& password);

    std::vector<UserAccount> listUsers(const UserAccount& actor, QString* errorOut = nullptr);
    bool createUser(const QString& login,
                    const QString& fullName,
                    UserRole role,
                    const QString& password,
                    const UserAccount& actor,
                    QString* errorOut = nullptr);
    bool setUserActive(int userId, bool active, const UserAccount& actor, QString* errorOut = nullptr);
    bool resetPassword(int userId, const QString& newPassword, const UserAccount& actor, QString* errorOut = nullptr);

private:
    AuthResult authenticateInternal(const QString& login, const QString& password, bool logAuditOnSuccess);

    DatabaseManager& m_db;
    AuditLogger& m_audit;
};

} // namespace uavlog
