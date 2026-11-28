#include "ui/FlightDialog.h"
#include "core/RbacGuard.h"
#include "ui/PreflightDialog.h"

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

FlightDialog::FlightDialog(AppContext& ctx, const UserAccount& actor, QWidget* parent)
    : QDialog(parent), m_ctx(ctx), m_actor(actor) {
    setWindowTitle("Регистрация полёта (5-факторный контроль допуска)");
    setMinimumWidth(840);

    auto* root = new QVBoxLayout(this);
    auto* cols = new QHBoxLayout();

    // Группа 1: Что и кто (соответствует мокапу SRS Рисунок 10)
    auto* grpWho = new QGroupBox("Что и кто", this);
    auto* formWho = new QFormLayout(grpWho);
    m_uavCombo = new QComboBox(this);
    m_opCombo = new QComboBox(this);
    m_batCombo = new QComboBox(this);

    for (const auto& u : m_ctx.fleet.loadUavs(false)) {
        m_uavCombo->addItem(
            QString("%1 — %2 [%3]").arg(QString::fromStdString(u.tailNumber),
                                        QString::fromStdString(u.typeName),
                                        QString::fromStdString(toRussianStatus(u.status))),
            u.id);
    }
    for (const auto& op : m_ctx.fleet.loadOperators(true)) {
        m_opCombo->addItem(
            QString("%1 (допуск до %2)").arg(QString::fromStdString(op.fullName),
                                             formatIsoDateRu(op.permitExpiryIso)),
            op.id);
    }
    // REQ-FUNC-016: АКБ с выработанным ресурсом (100 %) скрываются из списка выбора при регистрации полёта
    for (const auto& b : m_ctx.fleet.loadBatteries(true)) {
        m_batCombo->addItem(
            QString("%1 — %2 (%3/%4 циклов)")
                .arg(QString::fromStdString(b.inventoryNumber),
                     QString::fromStdString(b.modelName))
                .arg(b.usedCycles)
                .arg(b.maxCycles),
            b.id);
    }

    formWho->addRow("Борт:", m_uavCombo);
    formWho->addRow("Оператор:", m_opCombo);
    formWho->addRow("АКБ (исправные):", m_batCombo);
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
    m_taskEdit->setMinimumWidth(290);
    m_taskEdit->setCursorPosition(0);
    m_notesEdit = new QLineEdit(this);
    m_notesEdit->setPlaceholderText("Замечания по полёту (необязательно)");

    formWhen->addRow("Дата полёта:", m_dateEdit);
    formWhen->addRow("Время взлёта:", m_takeoffEdit);
    formWhen->addRow("Время посадки:", m_landingEdit);
    formWhen->addRow("Длительность (авто):", m_durationEdit);
    formWhen->addRow("Задача и район:", m_taskEdit);
    formWhen->addRow("Замечания:", m_notesEdit);
    cols->addWidget(grpWhen);

    root->addLayout(cols);

    // Индикатор 5-факторного допуска (REQ-INT-003, REQ-FUNC-006-3)
    m_admissionBanner = new QLabel(this);
    m_admissionBanner->setWordWrap(true);
    m_admissionBanner->setMargin(10);
    root->addWidget(m_admissionBanner);

    auto* btns = new QHBoxLayout();
    m_preflightQuickBtn = new QPushButton("Провести предполётный осмотр (F-15)...", this);
    m_preflightQuickBtn->hide();
    btns->addWidget(m_preflightQuickBtn);
    btns->addStretch();

    m_saveBtn = new QPushButton("Сохранить", this);
    m_saveBtn->setDefault(true);
    auto* cancelBtn = new QPushButton("Отмена", this);
    btns->addWidget(m_saveBtn);
    btns->addWidget(cancelBtn);
    root->addLayout(btns);

    // Клавиатурная навигация Tab (REQ-INT-004, мокап SRS Рисунок 10)
    setTabOrder(m_uavCombo, m_opCombo);
    setTabOrder(m_opCombo, m_batCombo);
    setTabOrder(m_batCombo, m_dateEdit);
    setTabOrder(m_dateEdit, m_takeoffEdit);
    setTabOrder(m_takeoffEdit, m_landingEdit);
    setTabOrder(m_landingEdit, m_taskEdit);
    setTabOrder(m_taskEdit, m_notesEdit);
    setTabOrder(m_notesEdit, m_saveBtn);
    setTabOrder(m_saveBtn, cancelBtn);

    connect(m_uavCombo, &QComboBox::currentIndexChanged, this, &FlightDialog::recomputeAndEvaluate);
    connect(m_opCombo, &QComboBox::currentIndexChanged, this, &FlightDialog::recomputeAndEvaluate);
    connect(m_batCombo, &QComboBox::currentIndexChanged, this, &FlightDialog::recomputeAndEvaluate);
    connect(m_dateEdit, &QDateEdit::dateChanged, this, &FlightDialog::recomputeAndEvaluate);
    connect(m_takeoffEdit, &QTimeEdit::timeChanged, this, &FlightDialog::recomputeAndEvaluate);
    connect(m_landingEdit, &QTimeEdit::timeChanged, this, &FlightDialog::recomputeAndEvaluate);
    connect(m_preflightQuickBtn, &QPushButton::clicked, this, &FlightDialog::onQuickPreflightClicked);
    connect(m_saveBtn, &QPushButton::clicked, this, &FlightDialog::onSaveClicked);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    recomputeAndEvaluate();
}

void FlightDialog::selectUavById(int uavId) {
    const int idx = m_uavCombo->findData(uavId);
    if (idx >= 0) m_uavCombo->setCurrentIndex(idx);
}

void FlightDialog::selectUavByIndex(int idx) {
    if (idx >= 0 && idx < m_uavCombo->count()) m_uavCombo->setCurrentIndex(idx);
}

void FlightDialog::selectOperatorByIndex(int idx) {
    if (idx >= 0 && idx < m_opCombo->count()) m_opCombo->setCurrentIndex(idx);
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
    dto.notes = m_notesEdit->text().trimmed();
    return dto;
}

void FlightDialog::recomputeAndEvaluate() {
    const auto dto = buildDto();
    if (dto.durationMinutes > 0) {
        m_durationEdit->setText(formatFlightMinutes(dto.durationMinutes));
    } else {
        m_durationEdit->setText("Ошибка времени");
    }

    const auto res = m_ctx.flights.evaluateAdmission(dto);
    m_saveBtn->setEnabled(res.allowed);

    const bool canPreflight = RbacGuard::hasPermission(m_actor.role, Permission::RegisterPreflightCheck);
    m_preflightQuickBtn->setVisible(!res.preflightOk && canPreflight && dto.uavId > 0);

    if (res.allowed) {
        m_admissionBanner->setStyleSheet(
            "background-color: #E6F4EA; border: 1px solid #1E8E3E; color: #137333; font-weight: bold; border-radius: 4px;");
        m_admissionBanner->setText(
            "ДОПУЩЕН К ПОЛЁТУ — все 5 факторов выполнены: борт исправен, регламент ТО в норме, "
            "допуск оператора действует на тип БЛА, ресурс АКБ в норме, предполётный осмотр (F-15) пройден.");
    } else {
        m_admissionBanner->setStyleSheet(
            "background-color: #FCE8E6; border: 1px solid #D93025; color: #A50E0E; font-weight: bold; border-radius: 4px;");
        QStringList lines;
        lines << "ОТКАЗ В ДОПУСКЕ (сохранение заблокировано):";
        for (const auto& r : res.reasons) {
            lines << " • " + QString::fromStdString(r);
        }
        m_admissionBanner->setText(lines.join("\n"));
    }
}

void FlightDialog::onQuickPreflightClicked() {
    PreflightDialog dlg(m_ctx, m_actor, m_uavCombo->currentData().toInt(), m_dateEdit->date(), this);
    if (dlg.exec() == QDialog::Accepted) {
        recomputeAndEvaluate();
    }
}

void FlightDialog::onSaveClicked() {
    QString err;
    if (!m_ctx.flights.registerFlight(buildDto(), m_actor, nullptr, &err)) {
        QMessageBox::warning(this, "Ошибка регистрации полёта", err);
        return;
    }
    accept();
}

} // namespace uavlog
