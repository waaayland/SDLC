#pragma once

#include "core/DomainTypes.h"
#include "services/AuthService.h"

#include <QDialog>
#include <optional>

class QLabel;
class QLineEdit;
class QPushButton;

namespace uavlog {

// Окно «Вход в систему» и модальная блокировка сеанса 15 мин (SRS Рисунок 7, REQ-FUNC-001, REQ-SEC-003/005)
class LoginDialog : public QDialog {
    Q_OBJECT
public:
    explicit LoginDialog(AuthService& authService,
                         const QString& lockedLogin = {},
                         QWidget* parent = nullptr);

    const std::optional<UserAccount>& authenticatedUser() const { return m_user; }

private slots:
    void onLoginClicked();

private:
    AuthService& m_authService;
    QString m_lockedLogin;
    QLineEdit* m_loginEdit = nullptr;
    QLineEdit* m_passEdit = nullptr;
    QLabel* m_errorLabel = nullptr;
    QPushButton* m_loginBtn = nullptr;
    std::optional<UserAccount> m_user;
};

} // namespace uavlog
