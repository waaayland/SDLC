#include "ui/FlightDialog.h"

#include <QComboBox>
#include <QDateEdit>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTimeEdit>
#include <QVBoxLayout>

namespace uavlog {

FlightDialog::FlightDialog(FleetService& fleetService, const UserAccount& actor, QWidget* parent)
    : QDialog(parent), m_fleetService(fleetService), m_actor(actor) {
    setWindowTitle("Регистрация полёта (5-факторный контроль допуска)");
    setMinimumWidth(720);

    auto* root = new QVBoxLayout(this);
    auto* cols = new QHBoxLayout();

    // Группа 1: Что и кто (соответствует мокапу SRS Рисунок 10)
    auto* grpWho = new QGroupBox("Что и кто", this);
    auto* formWho = new QFormLayout(grpWho);
    m_uavCombo = new QComboBox(this);
    m_opCombo = new QComboBox(this);
    m_batCombo = new QComboBox(this);

    const auto dash = m_fleetService.loadDashboard();
    for (const auto& u : dash.uavs) {
        if (u.status == UavStatus::Retired) continue;
        m_uavCombo->addItem(
            QString("%1 — %2 [%3]").arg(QString::fromStdString(u.tailNumber),
                                        QString::fromStdString(u.typeName),
                                        QString::fromStdString(toRussianStatus(u.status))),
            u.id);
    }
    for (const auto& op : m_fleetService.loadOperators()) {
        m_opCombo->addItem(
            QString("%1 (допуск до %2)").arg(QString::fromStdString(op.fullName),
                                             QString::fromStdString(op.permitExpiryIso)),
            op.id);
    }
    for (const auto& b : m_fleetService.loadBatteries(false)) {
        m_batCombo->addItem(
            QString("%1 (%2/%3 циклов)").arg(QString::fromStdString(b.inventoryNumber))
                .arg(b.usedCycles)
                .arg(b.maxCycles),
            b.id);
    }

    formWho->addRow("Борт:", m_uavCombo);
    formWho->addRow("Оператор:", m_opCombo);
    formWho->addRow("АКБ:", m_batCombo);
    cols->addWidget(grpWho);

    // Группа 2: Когда и где
    auto* grpWhen = new QGroupBox("Когда и где", this);
    auto* formWhen = new QFormLayout(grpWhen);
    m_dateEdit = new QDateEdit(QDate::currentDate(), this);
    m_dateEdit->setDisplayFormat("dd.MM.yyyy");
    m_dateEdit->setCalendarPopup(true);

    m_takeoffEdit = new QTimeEdit(QTime(10, 0), this);
    m_takeoffEdit->setDisplayFormat("HH:mm");
    m_landingEdit = new QTimeEdit(QTime(10, 45), this);
    m_landingEdit->setDisplayFormat("HH:mm");

    m_durationEdit = new QLineEdit(this);
    m_durationEdit->setReadOnly(true);
    m_taskEdit = new QLineEdit("Мониторинг периметра, полигон «Северный»", this);

    formWhen->addRow("Дата полёта:", m_dateEdit);
    formWhen->addRow("Время взлёта:", m_takeoffEdit);
    formWhen->addRow("Время посадки:", m_landingEdit);
    formWhen->addRow("Длительность (авто):", m_durationEdit);
    formWhen->addRow("Задача и район:", m_taskEdit);
    cols->addWidget(grpWhen);

    root->addLayout(cols);

    // Индикатор 5-факторного допуска (REQ-INT-003, REQ-FUNC-006-3)
    m_admissionBanner = new QLabel(this);
    m_admissionBanner->setWordWrap(true);
    m_admissionBanner->setMargin(10);
    root->addWidget(m_admissionBanner);

    auto* btns = new QHBoxLayout();
    btns->addStretch();
    m_saveBtn = new QPushButton("Сохранить", this);
    m_saveBtn->setDefault(true);
    auto* cancelBtn = new QPushButton("Отмена", this);
    btns->addWidget(m_saveBtn);
    btns->addWidget(cancelBtn);
    root->addLayout(btns);

    // Клавиатурная навигация Tab (REQ-INT-004)
    setTabOrder(m_uavCombo, m_opCombo);
    setTabOrder(m_opCombo, m_batCombo);
    setTabOrder(m_batCombo, m_dateEdit);
    setTabOrder(m_dateEdit, m_takeoffEdit);
    setTabOrder(m_takeoffEdit, m_landingEdit);
    setTabOrder(m_landingEdit, m_taskEdit);
    setTabOrder(m_taskEdit, m_saveBtn);
    setTabOrder(m_saveBtn, cancelBtn);

    connect(m_uavCombo, &QComboBox::currentIndexChanged, this, &FlightDialog::recomputeAndEvaluate);
    connect(m_opCombo, &QComboBox::currentIndexChanged, this, &FlightDialog::recomputeAndEvaluate);
    connect(m_batCombo, &QComboBox::currentIndexChanged, this, &FlightDialog::recomputeAndEvaluate);
    connect(m_dateEdit, &QDateEdit::dateChanged, this, &FlightDialog::recomputeAndEvaluate);
    connect(m_takeoffEdit, &QTimeEdit::timeChanged, this, &FlightDialog::recomputeAndEvaluate);
    connect(m_landingEdit, &QTimeEdit::timeChanged, this, &FlightDialog::recomputeAndEvaluate);
    connect(m_saveBtn, &QPushButton::clicked, this, &FlightDialog::onSaveClicked);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    recomputeAndEvaluate();
}

void FlightDialog::selectUavByIndex(int idx) {
    if (idx >= 0 && idx < m_uavCombo->count()) {
        m_uavCombo->setCurrentIndex(idx);
    }
}

void FlightDialog::selectOperatorByIndex(int idx) {
    if (idx >= 0 && idx < m_opCombo->count()) {
        m_opCombo->setCurrentIndex(idx);
    }
}

FlightInputDto FlightDialog::buildDto() const {
    FlightInputDto dto;
    dto.uavId = m_uavCombo->currentData().toInt();
    dto.operatorId = m_opCombo->currentData().toInt();
    dto.batteryId = m_batCombo->currentData().toInt();
    dto.flightDate = m_dateEdit->date();
    dto.takeoffTime = m_takeoffEdit->time().toString("HH:mm");
    dto.landingTime = m_landingEdit->time().toString("HH:mm");
    dto.durationMinutes = m_takeoffEdit->time().secsTo(m_landingEdit->time()) / 60;
    dto.taskAndArea = m_taskEdit->text().trimmed();
    return dto;
}

void FlightDialog::recomputeAndEvaluate() {
    const auto dto = buildDto();
    if (dto.durationMinutes > 0) {
        m_durationEdit->setText(QString("%1:%2")
                                    .arg(dto.durationMinutes / 60)
                                    .arg(dto.durationMinutes % 60, 2, 10, QChar('0')));
    } else {
        m_durationEdit->setText("Ошибка времени");
    }

    const auto res = m_fleetService.evaluateFlightAdmission(dto);
    m_saveBtn->setEnabled(res.allowed);
    if (res.allowed) {
        m_admissionBanner->setStyleSheet(
            "background-color: #E6F4EA; border: 1px solid #1E8E3E; color: #137333; font-weight: bold;");
        m_admissionBanner->setText(
            "ДОПУЩЕН К ПОЛЁТУ — все 5 факторов выполнены: борт исправен, ТО в норме, "
            "допуск оператора действует, ресурс АКБ в норме, предполётный осмотр (F-15) пройден.");
    } else {
        m_admissionBanner->setStyleSheet(
            "background-color: #FCE8E6; border: 1px solid #D93025; color: #A50E0E; font-weight: bold;");
        QStringList lines;
        lines << "ОТКАЗ В ДОПУСКЕ (сохранение заблокировано):";
        for (const auto& r : res.reasons) {
            lines << " • " + QString::fromStdString(r);
        }
        m_admissionBanner->setText(lines.join("\n"));
    }
}

void FlightDialog::onSaveClicked() {
    QString err;
    if (!m_fleetService.registerFlight(buildDto(), m_actor, &err)) {
        QMessageBox::warning(this, "Ошибка регистрации полёта", err);
        return;
    }
    accept();
}

} // namespace uavlog
