#pragma once

#include "services/AuthService.h"
#include <QDialog>

class QLabel;
class QLineEdit;
class QPushButton;

namespace uavlog {

class LoginDialog : public QDialog {
    Q_OBJECT
public:
    explicit LoginDialog(AuthService& authService, QWidget* parent = nullptr);
    const std::optional<UserAccount>& authenticatedUser() const { return m_user; }

private slots:
    void onLoginClicked();

private:
    AuthService& m_authService;
    QLineEdit* m_loginEdit = nullptr;
    QLineEdit* m_passEdit = nullptr;
    QLabel* m_errorLabel = nullptr;
    QPushButton* m_loginBtn = nullptr;
    std::optional<UserAccount> m_user;
};

} // namespace uavlog
