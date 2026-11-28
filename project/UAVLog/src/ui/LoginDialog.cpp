#include "ui/LoginDialog.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace uavlog {

LoginDialog::LoginDialog(AuthService& authService, const QString& lockedLogin, QWidget* parent)
    : QDialog(parent), m_authService(authService), m_lockedLogin(lockedLogin) {
    const bool lockMode = !m_lockedLogin.isEmpty();
    setWindowTitle(lockMode ? "UAVLog 1.0 — Сеанс заблокирован (15 мин бездействия)"
                            : "UAVLog 1.0 — Вход в систему");
    setMinimumWidth(420);
    setModal(true);

    auto* root = new QVBoxLayout(this);

    auto* title = new QLabel(
        lockMode ? "<b>Сеанс заблокирован из-за бездействия</b><br/>"
                   "Введите пароль текущего пользователя для продолжения работы без потери данных."
                 : "<b>UAVLog 1.0 — Журнал эксплуатации парка БЛА</b><br/>"
                   "Аутентификация пользователя (PBKDF2-HMAC-SHA256, RBAC)",
        this);
    title->setWordWrap(true);
    title->setStyleSheet("padding: 6px; background: #F0F4F8; border: 1px solid #C5D5E4; border-radius: 4px;");
    root->addWidget(title);

    auto* form = new QFormLayout();
    m_loginEdit = new QLineEdit(this);
    m_loginEdit->setPlaceholderText("Например: admin, tech, operator, head");
    if (lockMode) {
        m_loginEdit->setText(m_lockedLogin);
        m_loginEdit->setReadOnly(true);
    }

    m_passEdit = new QLineEdit(this);
    m_passEdit->setEchoMode(QLineEdit::Password);
    m_passEdit->setPlaceholderText("Введите пароль (минимум 8 символов)");

    form->addRow("Логин:", m_loginEdit);
    form->addRow("Пароль:", m_passEdit);
    root->addLayout(form);

    m_errorLabel = new QLabel(this);
    m_errorLabel->setWordWrap(true);
    m_errorLabel->setStyleSheet("color: #A50E0E; font-weight: bold;");
    m_errorLabel->hide();
    root->addWidget(m_errorLabel);

    if (!lockMode) {
        auto* hint = new QLabel(
            "<span style='color:#4A5568;font-size:11px;'>Демо-учётные записи: "
            "<b>admin</b> / Admin1234! &nbsp;|&nbsp; <b>tech</b> / Tech1234! &nbsp;|&nbsp; "
            "<b>operator</b> / Oper1234! &nbsp;|&nbsp; <b>head</b> / Head1234!</span>",
            this);
        hint->setWordWrap(true);
        root->addWidget(hint);
    }

    auto* btns = new QHBoxLayout();
    btns->addStretch();
    m_loginBtn = new QPushButton(lockMode ? "Разблокировать" : "Войти", this);
    m_loginBtn->setDefault(true);
    auto* exitBtn = new QPushButton(lockMode ? "Завершить работу" : "Выход", this);
    btns->addWidget(m_loginBtn);
    btns->addWidget(exitBtn);
    root->addLayout(btns);

    // Порядок Tab согласно мокапу SRS Рисунок 7 (REQ-INT-004)
    setTabOrder(m_loginEdit, m_passEdit);
    setTabOrder(m_passEdit, m_loginBtn);
    setTabOrder(m_loginBtn, exitBtn);

    if (lockMode) {
        m_passEdit->setFocus();
    } else {
        m_loginEdit->setFocus();
    }

    connect(m_loginBtn, &QPushButton::clicked, this, &LoginDialog::onLoginClicked);
    connect(exitBtn, &QPushButton::clicked, this, &QDialog::reject);
}

void LoginDialog::onLoginClicked() {
    const QString login = m_loginEdit->text().trimmed();
    const QString pass = m_passEdit->text();

    const AuthResult res = m_lockedLogin.isEmpty()
        ? m_authService.authenticate(login, pass)
        : m_authService.verifyCurrentUserPassword(m_lockedLogin, pass);

    if (!res.success || !res.user.has_value()) {
        m_errorLabel->setText(res.errorMessage);
        m_errorLabel->show();
        m_passEdit->clear();
        m_passEdit->setFocus();
        return;
    }

    m_user = res.user;
    accept();
}

} // namespace uavlog
