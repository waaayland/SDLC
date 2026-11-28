#include "ui/FaultsDialog.h"
#include "core/RbacGuard.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateEdit>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace uavlog {

FaultsDialog::FaultsDialog(AppContext& ctx,
                           const UserAccount& actor,
                           int preselectUavId,
                           QWidget* parent)
    : QDialog(parent), m_ctx(ctx), m_actor(actor) {
    setWindowTitle("Журнал неисправностей и отказов БЛА");
    setMinimumSize(820, 540);

    auto* root = new QVBoxLayout(this);

    // Блок регистрации новой неисправности
    auto* grpNew = new QGroupBox("Зарегистрировать неисправность", this);
    auto* form = new QFormLayout(grpNew);

    m_uavCombo = new QComboBox(this);
    for (const auto& u : m_ctx.fleet.loadUavs(false)) {
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

    m_dateEdit = new QDateEdit(QDate::currentDate(), this);
    m_dateEdit->setDisplayFormat("dd.MM.yyyy");
    m_dateEdit->setCalendarPopup(true);

    m_descEdit = new QLineEdit(this);
    m_descEdit->setPlaceholderText("Подробно опишите выявленный дефект или отказ...");

    m_criticalCheck = new QCheckBox("Критичная неисправность (немедленно переводит борт в «Неисправен» и блокирует вылет)", this);
    m_criticalCheck->setChecked(true);

    auto* reportBtn = new QPushButton("Зарегистрировать неисправность", this);
    reportBtn->setEnabled(RbacGuard::hasPermission(m_actor.role, Permission::ReportFault));

    form->addRow("Борт БЛА:", m_uavCombo);
    form->addRow("Дата выявления:", m_dateEdit);
    form->addRow("Описание дефекта:", m_descEdit);
    form->addRow("", m_criticalCheck);
    form->addRow("", reportBtn);
    root->addWidget(grpNew);

    // Таблица всех неисправностей
    m_table = new QTableWidget(this);
    m_table->setColumnCount(6);
    m_table->setHorizontalHeaderLabels({"Дата", "Борт", "Критичность", "Описание", "Статус", "Выполненные работы"});
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    root->addWidget(m_table, 1);

    auto* btns = new QHBoxLayout();
    m_resolveBtn = new QPushButton("Отметить устранение выбранной неисправности...", this);
    m_resolveBtn->setVisible(RbacGuard::hasPermission(m_actor.role, Permission::ResolveFault));
    btns->addWidget(m_resolveBtn);
    btns->addStretch();
    auto* closeBtn = new QPushButton("Закрыть", this);
    btns->addWidget(closeBtn);
    root->addLayout(btns);

    connect(reportBtn, &QPushButton::clicked, this, &FaultsDialog::onReportClicked);
    connect(m_resolveBtn, &QPushButton::clicked, this, &FaultsDialog::onResolveClicked);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);

    refreshTable();
}

void FaultsDialog::refreshTable() {
    m_faults = m_ctx.maint.loadFaults(0, false);
    m_table->setRowCount(static_cast<int>(m_faults.size()));
    for (int i = 0; i < static_cast<int>(m_faults.size()); ++i) {
        const auto& f = m_faults[i];
        m_table->setItem(i, 0, new QTableWidgetItem(formatIsoDateRu(f.reportedDateIso)));
        m_table->setItem(i, 1, new QTableWidgetItem(QString::fromStdString(f.uavTailNumber)));
        auto* critItem = new QTableWidgetItem(f.isCritical ? "КРИТИЧНАЯ" : "Некритичная");
        if (f.isCritical && f.isOpen()) {
            critItem->setBackground(QBrush(QColor("#FCE8E6")));
            critItem->setForeground(QBrush(QColor("#A50E0E")));
        }
        m_table->setItem(i, 2, critItem);
        m_table->setItem(i, 3, new QTableWidgetItem(QString::fromStdString(f.description)));
        auto* stItem = new QTableWidgetItem(f.isOpen() ? "Открыта" : ("Устранена " + formatIsoDateRu(f.resolvedDateIso)));
        stItem->setForeground(QBrush(f.isOpen() ? QColor("#A50E0E") : QColor("#137333")));
        m_table->setItem(i, 4, stItem);
        m_table->setItem(i, 5, new QTableWidgetItem(QString::fromStdString(f.resolutionWorks)));
    }
}

void FaultsDialog::onReportClicked() {
    QString err;
    if (!m_ctx.maint.reportFault(
            m_uavCombo->currentData().toInt(),
            m_dateEdit->date(),
            m_descEdit->text(),
            m_criticalCheck->isChecked(),
            m_actor,
            nullptr,
            &err)) {
        QMessageBox::warning(this, "Ошибка регистрации", err);
        return;
    }
    m_descEdit->clear();
    refreshTable();
    QMessageBox::information(this, "Неисправность зарегистрирована",
                             m_criticalCheck->isChecked()
                                 ? "Критичная неисправность сохранена. Борт переведён в состояние «Неисправен»."
                                 : "Некритичная неисправность сохранена в журнале.");
}

void FaultsDialog::onResolveClicked() {
    const int row = m_table->currentRow();
    if (row < 0 || row >= static_cast<int>(m_faults.size())) {
        QMessageBox::information(this, "Выбор записи", "Выберите открытую неисправность в таблице.");
        return;
    }
    const auto& f = m_faults[row];
    if (!f.isOpen()) {
        QMessageBox::information(this, "Уже устранена", "Эта неисправность уже отмечена как устранённая.");
        return;
    }

    bool ok = false;
    const QString works = QInputDialog::getMultiLineText(
        this, "Устранение неисправности — " + QString::fromStdString(f.uavTailNumber),
        "Опишите выполненные ремонтные работы (обязательно по REQ-FUNC-011):",
        "", &ok);
    if (!ok) return;

    QString err;
    if (!m_ctx.maint.resolveFault(f.id, QDate::currentDate(), works, m_actor, &err)) {
        QMessageBox::warning(this, "Ошибка устранения", err);
        return;
    }
    refreshTable();
    QMessageBox::information(this, "Неисправность устранена",
                             "Отметка об устранении сохранена. Состояние борта пересчитано конечным автоматом.");
}

} // namespace uavlog
