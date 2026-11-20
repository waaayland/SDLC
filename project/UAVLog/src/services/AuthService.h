#pragma once

#include "core/DomainTypes.h"
#include "infra/AuditLogger.h"
#include "infra/DatabaseManager.h"

#include <QString>
#include <optional>

namespace uavlog {

struct LoginOutcome {
    bool success = false;
    bool locked = false;
    QString errorMessage;
    std::optional<UserAccount> user;
};

class AuthService {
public:
    static constexpr int kMaxFailedAttempts = 5;   // REQ-SEC-005
    static constexpr int kLockoutMinutes = 5;

    AuthService(DatabaseManager& db, AuditLogger& audit);

    LoginOutcome authenticate(const QString& username, const QString& password);

private:
    DatabaseManager& m_db;
    AuditLogger& m_audit;
};

} // namespace uavlog
