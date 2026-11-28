#include "ui/MaintenanceDialog.h"

#include <QComboBox>
#include <QDateEdit>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace uavlog {

MaintenanceDialog::MaintenanceDialog(AppContext& ctx,
                                     const UserAccount& actor,
                                     int preselectUavId,
                                     QWidget* parent)
    : QDialog(parent), m_ctx(ctx), m_actor(actor) {
    setWindowTitle("Выполнение технического обслуживания (ТО)");
    setMinimumSize(740, 520);

    auto* root = new QVBoxLayout(this);

    // Группа 1: Реквизиты ТО (мокап SRS Рисунок 11)
    auto* grpHdr = new QGroupBox("Реквизиты ТО", this);
    auto* form = new QFormLayout(grpHdr);

    m_uavCombo = new QComboBox(this);
    m_uavs = m_ctx.fleet.loadUavs(false);
    for (const auto& u : m_uavs) {
        m_uavCombo->addItem(
            QString("%1 — %2 [%3]").arg(QString::fromStdString(u.tailNumber),
                                        QString::fromStdString(u.typeName),
                                        QString::fromStdString(toRussianStatus(u.status))),
            u.id);
    }
    if (preselectUavId > 0) {
        const int idx = m_uavCombo->findData(preselectUavId);
        if (idx >= 0) m_uavCombo->setCurrentIndex(idx);
    }

    m_ruleCombo = new QComboBox(this);
    m_dateEdit = new QDateEdit(QDate::currentDate(), this);
    m_dateEdit->setDisplayFormat("dd.MM.yyyy");
    m_dateEdit->setCalendarPopup(true);

    m_performerEdit = new QLineEdit(QString::fromStdString(m_actor.fullName), this);
    m_flightHoursEdit = new QLineEdit(this);
    m_flightHoursEdit->setReadOnly(true);

    form->addRow("Борт:", m_uavCombo);
    form->addRow("Вид ТО по регламенту:", m_ruleCombo);
    form->addRow("Дата выполнения:", m_dateEdit);
    form->addRow("Исполнитель:", m_performerEdit);
    form->addRow("Наработка на момент ТО (авто):", m_flightHoursEdit);
    root->addWidget(grpHdr);

    // Таблица работ (Работа | Выполнено | Примечание — мокап SRS Рисунок 11)
    m_worksTable = new QTableWidget(this);
    m_worksTable->setColumnCount(3);
    m_worksTable->setHorizontalHeaderLabels({"Работа по регламенту", "Выполнено", "Примечание (обязательно при невыполнении)"});
    m_worksTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_worksTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_worksTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_worksTable->setAlternatingRowColors(true);
    root->addWidget(m_worksTable, 1);

    auto* notesForm = new QFormLayout();
    m_notesEdit = new QLineEdit(this);
    m_notesEdit->setPlaceholderText("Общее примечание по результатам ТО...");
    notesForm->addRow("Примечание:", m_notesEdit);
    root->addLayout(notesForm);

    auto* btns = new QHBoxLayout();
    m_startBtn = new QPushButton("Перевести борт «На ТО»", this);
    btns->addWidget(m_startBtn);
    btns->addStretch();
    m_saveBtn = new QPushButton("Сохранить выполнение ТО", this);
    m_saveBtn->setDefault(true);
    auto* cancelBtn = new QPushButton("Отмена", this);
    btns->addWidget(m_saveBtn);
    btns->addWidget(cancelBtn);
    root->addLayout(btns);

    // Порядок Tab по мокапу SRS Рисунок 11
    setTabOrder(m_uavCombo, m_ruleCombo);
    setTabOrder(m_ruleCombo, m_dateEdit);
    setTabOrder(m_dateEdit, m_performerEdit);
    setTabOrder(m_performerEdit, m_worksTable);
    setTabOrder(m_worksTable, m_notesEdit);
    setTabOrder(m_notesEdit, m_saveBtn);

    connect(m_uavCombo, &QComboBox::currentIndexChanged, this, &MaintenanceDialog::onUavChanged);
    connect(m_ruleCombo, &QComboBox::currentIndexChanged, this, &MaintenanceDialog::onRuleChanged);
    connect(m_startBtn, &QPushButton::clicked, this, &MaintenanceDialog::onStartMaintClicked);
    connect(m_saveBtn, &QPushButton::clicked, this, &MaintenanceDialog::onSaveClicked);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    onUavChanged();
}

void MaintenanceDialog::onUavChanged() {
    m_ruleCombo->clear();
    m_rulesForType.clear();
    m_worksTable->setRowCount(0);

    const int uavId = m_uavCombo->currentData().toInt();
    const Uav* selected = nullptr;
    for (const auto& u : m_uavs) {
        if (u.id == uavId) { selected = &u; break; }
    }
    if (!selected) return;

    m_flightHoursEdit->setText(formatFlightMinutes(selected->totalFlightMin) +
                               QString(" (%1 полётов)").arg(selected->totalFlights));
    m_startBtn->setEnabled(selected->status == UavStatus::Serviceable ||
                           selected->status == UavStatus::DueMaintenance);

    m_rulesForType = m_ctx.fleet.loadMaintenanceRules(selected->uavTypeId);
    for (const auto& r : m_rulesForType) {
        QString period;
        if (r.intervalHours.has_value()) period += QString("%1 ч").arg(*r.intervalHours, 0, 'f', 0);
        if (r.intervalDays.has_value()) {
            if (!period.isEmpty()) period += " / ";
            period += QString("%1 дн.").arg(*r.intervalDays);
        }
        m_ruleCombo->addItem(QString("%1 (%2)").arg(QString::fromStdString(r.title), period), r.id);
    }
    onRuleChanged();
}

void MaintenanceDialog::onRuleChanged() {
    m_worksTable->setRowCount(0);
    const int ruleId = m_ruleCombo->currentData().toInt();
    for (const auto& r : m_rulesForType) {
        if (r.id != ruleId) continue;
        m_worksTable->setRowCount(static_cast<int>(r.tasks.size()));
        for (int i = 0; i < static_cast<int>(r.tasks.size()); ++i) {
            auto* wItem = new QTableWidgetItem(QString::fromStdString(r.tasks[i]));
            wItem->setFlags(wItem->flags() & ~Qt::ItemIsEditable);
            m_worksTable->setItem(i, 0, wItem);

            auto* chkItem = new QTableWidgetItem("Да");
            chkItem->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            chkItem->setCheckState(Qt::Checked);
            m_worksTable->setItem(i, 1, chkItem);

            m_worksTable->setItem(i, 2, new QTableWidgetItem(""));
        }
        break;
    }
}

void MaintenanceDialog::onStartMaintClicked() {
    QString err;
    if (!m_ctx.maint.startMaintenance(m_uavCombo->currentData().toInt(), m_actor, &err)) {
        QMessageBox::warning(this, "Перевод на ТО", err);
        return;
    }
    QMessageBox::information(this, "Статус обновлён",
                             "Борт переведён в состояние «На ТО» (вылеты заблокированы до завершения работ).");
    m_startBtn->setEnabled(false);
}

void MaintenanceDialog::onSaveClicked() {
    std::vector<MaintenanceWorkCheck> works;
    works.reserve(m_worksTable->rowCount());
    for (int i = 0; i < m_worksTable->rowCount(); ++i) {
        MaintenanceWorkCheck w;
        w.taskText = m_worksTable->item(i, 0)->text();
        w.completed = m_worksTable->item(i, 1)->checkState() == Qt::Checked;
        w.note = m_worksTable->item(i, 2)->text().trimmed();
        works.push_back(std::move(w));
    }

    QString err;
    if (!m_ctx.maint.registerMaintenance(
            m_uavCombo->currentData().toInt(),
            m_ruleCombo->currentData().toInt(),
            m_dateEdit->date(),
            m_performerEdit->text(),
            works,
            m_notesEdit->text(),
            m_actor,
            nullptr,
            &err)) {
        QMessageBox::warning(this, "Ошибка сохранения ТО", err);
        return;
    }

    QMessageBox::information(this, "ТО зарегистрировано",
                             "Выполненное ТО сохранено; наработка по данному виду ТО обнулена, статус борта актуализирован.");
    accept();
}

} // namespace uavlog
