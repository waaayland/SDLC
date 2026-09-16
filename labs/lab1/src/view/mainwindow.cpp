#include "view/mainwindow.h"
#include "ui_mainwindow.h"

#include "controller/passwordcontroller.h"
#include "model/passwordmodel.h"

#include <QMessageBox>

MainWindow::MainWindow(PasswordModel &model, PasswordController &controller, QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_model(model)
    , m_controller(controller)
{
    ui->setupUi(this);

    // Представление подписывается на модель (Observer).
    m_model.attach(this);

    // Действие пользователя уходит в контроллер, а не обрабатывается здесь.
    connect(ui->buttonEnterData, &QPushButton::clicked,
            &m_controller, &PasswordController::requestInput);

    showInitialState();
}

MainWindow::~MainWindow()
{
    m_model.detach(this);
    delete ui;
}

void MainWindow::showInitialState()
{
    if (m_model.hasResult()) {
        onSettingsChanged(m_model.settings());
        onPasswordsChanged(m_model.passwords());
    } else {
        ui->labelLengthValue->setText(QStringLiteral("—"));
        ui->labelCountValue->setText(QStringLiteral("—"));
        ui->labelSetsValue->setText(QStringLiteral("—"));
        ui->listPasswords->clear();
        statusBar()->showMessage(
            QStringLiteral("Нажмите «Ввести данные», чтобы задать параметры генерации."));
    }
}

void MainWindow::onSettingsChanged(const PasswordSettings &settings)
{
    ui->labelLengthValue->setText(QString::number(settings.length));
    ui->labelCountValue->setText(QString::number(settings.count));
    ui->labelSetsValue->setText(settings.enabledSetNames().join(QStringLiteral(", ")));
}

void MainWindow::onPasswordsChanged(const QStringList &passwords)
{
    ui->listPasswords->clear();
    ui->listPasswords->addItems(passwords);
    statusBar()->showMessage(
        QStringLiteral("Сгенерировано паролей: %1").arg(passwords.size()));
}

void MainWindow::onInputRejected(const QString &message)
{
    statusBar()->showMessage(QStringLiteral("Ввод отклонён: данные некорректны."));
    QMessageBox::warning(this, QStringLiteral("Некорректные данные"), message);
}
