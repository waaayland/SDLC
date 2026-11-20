#include "ui/LoginDialog.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace uavlog {

LoginDialog::LoginDialog(AuthService& authService, QWidget* parent)
    : QDialog(parent), m_authService(authService) {
    setWindowTitle("UAVLog — Вход в систему");
    setMinimumWidth(380);

    auto* root = new QVBoxLayout(this);
    auto* title = new QLabel("<b>UAVLog 1.0</b> — Журнал эксплуатации парка БЛА", this);
    title->setAlignment(Qt::AlignCenter);
    root->addWidget(title);

    auto* form = new QFormLayout();
    m_loginEdit = new QLineEdit(this);
    m_loginEdit->setPlaceholderText("admin / tech / operator / head");
    m_passEdit = new QLineEdit(this);
    m_passEdit->setEchoMode(QLineEdit::Password);
    m_passEdit->setPlaceholderText("Введите пароль (≥ 8 символов)");

    form->addRow("Логин:", m_loginEdit);
    form->addRow("Пароль:", m_passEdit);
    root->addLayout(form);

    m_errorLabel = new QLabel(this);
    m_errorLabel->setStyleSheet("color: #B00020; font-weight: bold;");
    m_errorLabel->setWordWrap(true);
    m_errorLabel->hide();
    root->addWidget(m_errorLabel);

    auto* btns = new QHBoxLayout();
    btns->addStretch();
    m_loginBtn = new QPushButton("Войти", this);
    m_loginBtn->setDefault(true);
    auto* exitBtn = new QPushButton("Выход", this);
    btns->addWidget(m_loginBtn);
    btns->addWidget(exitBtn);
    root->addLayout(btns);

    setTabOrder(m_loginEdit, m_passEdit);
    setTabOrder(m_passEdit, m_loginBtn);
    setTabOrder(m_loginBtn, exitBtn);

    connect(m_loginBtn, &QPushButton::clicked, this, &LoginDialog::onLoginClicked);
    connect(exitBtn, &QPushButton::clicked, this, &QDialog::reject);
}

void LoginDialog::onLoginClicked() {
    const auto outcome = m_authService.authenticate(m_loginEdit->text(), m_passEdit->text());
    if (!outcome.success) {
        m_errorLabel->setText(outcome.errorMessage);
        m_errorLabel->show();
        m_passEdit->clear();
        m_passEdit->setFocus();
        return;
    }
    m_user = outcome.user;
    accept();
}

} // namespace uavlog
