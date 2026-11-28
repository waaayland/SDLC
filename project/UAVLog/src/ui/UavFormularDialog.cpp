#include "ui/UavFormularDialog.h"
#include "core/RbacGuard.h"
#include "ui/FaultsDialog.h"
#include "ui/MaintenanceDialog.h"

#include <QComboBox>
#include <QDateEdit>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

namespace uavlog {

UavFormularDialog::UavFormularDialog(AppContext& ctx,
                                     const UserAccount& actor,
                                     int uavId,
                                     QWidget* parent)
    : QDialog(parent), m_ctx(ctx), m_actor(actor), m_uavId(uavId) {
    setWindowTitle("Электронный формуляр БЛА");
    setMinimumSize(900, 620);

    auto* root = new QVBoxLayout(this);

    // Верхняя часть: Паспорт борта + Индикатор состояния (мокап SRS Рисунок 9)
    auto* topRow = new QHBoxLayout();
    auto* grpPass = new QGroupBox("Паспорт борта", this);
    auto* passLayout = new QVBoxLayout(grpPass);
    m_passportLabel = new QLabel(this);
    m_passportLabel->setWordWrap(true);
    m_passportLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    passLayout->addWidget(m_passportLabel);
    topRow->addWidget(grpPass, 3);

    auto* grpState = new QGroupBox("Состояние и остатки до ТО", this);
    auto* stateLayout = new QVBoxLayout(grpState);
    m_statusBanner = new QLabel(this);
    m_statusBanner->setWordWrap(true);
    m_statusBanner->setMargin(8);
    stateLayout->addWidget(m_statusBanner);
    topRow->addWidget(grpState, 2);
    root->addLayout(topRow);

    // 4 вкладки журналов (мокап SRS Рисунок 9: Полёты, ТО, Неисправности, АКБ и нагрузка)
    m_tabs = new QTabWidget(this);

    m_flightsTable = new QTableWidget(this);
    m_flightsTable->setColumnCount(6);
    m_flightsTable->setHorizontalHeaderLabels({"Дата", "Взлёт–Посадка", "Длительность", "Оператор", "АКБ", "Задача и район"});
    m_flightsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_flightsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_flightsTable->setAlternatingRowColors(true);
    m_tabs->addTab(m_flightsTable, "Полёты");

    m_maintTable = new QTableWidget(this);
    m_maintTable->setColumnCount(5);
    m_maintTable->setHorizontalHeaderLabels({"Дата ТО", "Вид ТО", "Налёт на момент ТО", "Исполнитель", "Примечание"});
    m_maintTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_maintTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_maintTable->setAlternatingRowColors(true);
    m_tabs->addTab(m_maintTable, "ТО");

    m_faultsTable = new QTableWidget(this);
    m_faultsTable->setColumnCount(5);
    m_faultsTable->setHorizontalHeaderLabels({"Дата", "Критичность", "Описание", "Статус", "Выполненные работы"});
    m_faultsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_faultsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_faultsTable->setAlternatingRowColors(true);
    m_tabs->addTab(m_faultsTable, "Неисправности");

    auto* tab4 = new QWidget(this);
    auto* tab4Lay = new QVBoxLayout(tab4);
    tab4Lay->addWidget(new QLabel("<b>Закреплённые аккумуляторные батареи (АКБ):</b>", tab4));
    m_akbPreflightTable = new QTableWidget(tab4);
    m_akbPreflightTable->setColumnCount(5);
    m_akbPreflightTable->setHorizontalHeaderLabels({"Инв. номер", "Модель", "Циклы", "Износ, %", "Статус"});
    m_akbPreflightTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_akbPreflightTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tab4Lay->addWidget(m_akbPreflightTable, 1);

    tab4Lay->addWidget(new QLabel("<b>История предполётных осмотров (модуль F-15):</b>", tab4));
    m_preflightTable = new QTableWidget(tab4);
    m_preflightTable->setColumnCount(4);
    m_preflightTable->setHorizontalHeaderLabels({"Дата осмотра", "Результат", "Проверяющий", "Заключение / замечания"});
    m_preflightTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_preflightTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tab4Lay->addWidget(m_preflightTable, 1);
    m_tabs->addTab(tab4, "АКБ, нагрузка и осмотры");

    root->addWidget(m_tabs, 1);

    // Нижняя панель кнопок (мокап SRS Рисунок 9 + карта окон Рисунок 6)
    auto* btns = new QHBoxLayout();
    m_editBtn = new QPushButton("Редактировать", this);
    m_maintBtn = new QPushButton("Внести ТО...", this);
    m_faultBtn = new QPushButton("Неисправность...", this);
    m_decommBtn = new QPushButton("Списать борт", this);
    auto* pdfBtn = new QPushButton("Экспорт PDF A4", this);
    auto* printBtn = new QPushButton("Печать формуляра", this);
    auto* closeBtn = new QPushButton("Закрыть", this);

    m_editBtn->setVisible(RbacGuard::hasPermission(m_actor.role, Permission::ManageDictionaries));
    m_decommBtn->setVisible(RbacGuard::hasPermission(m_actor.role, Permission::DecommissionUav));
    m_maintBtn->setVisible(RbacGuard::hasPermission(m_actor.role, Permission::RegisterMaintenance));
    m_faultBtn->setVisible(RbacGuard::hasPermission(m_actor.role, Permission::ReportFault));

    btns->addWidget(m_editBtn);
    btns->addWidget(m_maintBtn);
    btns->addWidget(m_faultBtn);
    btns->addWidget(m_decommBtn);
    btns->addStretch();
    btns->addWidget(pdfBtn);
    btns->addWidget(printBtn);
    btns->addWidget(closeBtn);
    root->addLayout(btns);

    connect(m_editBtn, &QPushButton::clicked, this, &UavFormularDialog::onEditPassportClicked);
    connect(m_decommBtn, &QPushButton::clicked, this, &UavFormularDialog::onDecommissionClicked);
    connect(m_maintBtn, &QPushButton::clicked, this, &UavFormularDialog::onMaintClicked);
    connect(m_faultBtn, &QPushButton::clicked, this, &UavFormularDialog::onFaultClicked);
    connect(pdfBtn, &QPushButton::clicked, this, &UavFormularDialog::onExportPdfClicked);
    connect(printBtn, &QPushButton::clicked, this, &UavFormularDialog::onPrintClicked);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);

    reloadFormular();
}

void UavFormularDialog::reloadFormular() {
    m_data = m_ctx.fleet.loadFormular(m_uavId);
    const auto& u = m_data.uav;
    setWindowTitle("Формуляр БЛА — " + QString::fromStdString(u.tailNumber));

    m_passportLabel->setText(
        QString("<b>Бортовой номер:</b> %1 &nbsp;&nbsp;|&nbsp;&nbsp; <b>Тип:</b> %2<br/>"
                "<b>Заводской номер:</b> %3 &nbsp;&nbsp;|&nbsp;&nbsp; <b>Дата ввода:</b> %4<br/>"
                "<b>Штатная полезная нагрузка:</b> %5<br/>"
                "<b>Суммарный налёт:</b> %6 (%7 полётов) &nbsp;&nbsp;|&nbsp;&nbsp; <b>Последний полёт:</b> %8<br/>"
                "<b>Примечание:</b> %9")
            .arg(QString::fromStdString(u.tailNumber).toHtmlEscaped(),
                 QString::fromStdString(u.typeName).toHtmlEscaped(),
                 QString::fromStdString(u.serialNumber).toHtmlEscaped(),
                 formatIsoDateRu(u.commissionDateIso),
                 QString::fromStdString(u.standardPayload).toHtmlEscaped(),
                 formatFlightMinutes(u.totalFlightMin))
            .arg(u.totalFlights)
            .arg(formatIsoDateRu(u.lastFlightDateIso),
                 QString::fromStdString(u.notes).toHtmlEscaped()));

    QStringList remLines;
    bool anyWarn = false;
    bool anyOver = false;
    for (const auto& r : m_data.remainders) {
        if (r.isOverdue) anyOver = true;
        if (r.isWarning) anyWarn = true;
        QString part = QString::fromStdString(r.ruleTitle) + ": ";
        if (r.remainingHours.has_value()) part += QString("%1 ч").arg(*r.remainingHours, 0, 'f', 1);
        if (r.remainingDays.has_value()) {
            if (r.remainingHours.has_value()) part += " / ";
            part += QString("%1 дн.").arg(*r.remainingDays);
        }
        if (r.isOverdue) part += " [ПРОСРОЧЕНО]";
        else if (r.isWarning) part += " [СКОРО]";
        remLines << part;
    }

    QString style;
    if (u.status == UavStatus::Retired) {
        style = "background:#ECEFF1;border:1px solid #90A4AE;color:#37474F;border-radius:4px;";
    } else if (u.status == UavStatus::Faulty || u.status == UavStatus::DueMaintenance || anyOver) {
        style = "background:#FCE8E6;border:1px solid #D93025;color:#A50E0E;border-radius:4px;";
    } else if (u.status == UavStatus::InMaintenance || anyWarn) {
        style = "background:#FFF8E1;border:1px solid #E6A800;color:#795500;border-radius:4px;";
    } else {
        style = "background:#E6F4EA;border:1px solid #1E8E3E;color:#137333;border-radius:4px;";
    }
    m_statusBanner->setStyleSheet(style);
    m_statusBanner->setText(
        QString("<b>Состояние: %1</b><br/>%2")
            .arg(QString::fromStdString(toRussianStatus(u.status)),
                 remLines.isEmpty() ? "Регламенты ТО не заданы" : remLines.join("<br/>")));

    const bool isRetired = (u.status == UavStatus::Retired);
    m_editBtn->setEnabled(!isRetired);
    m_decommBtn->setEnabled(!isRetired);
    m_maintBtn->setEnabled(!isRetired);
    m_faultBtn->setEnabled(!isRetired);

    // Таблица 1: Полёты
    m_flightsTable->setRowCount(static_cast<int>(m_data.flights.size()));
    for (int i = 0; i < static_cast<int>(m_data.flights.size()); ++i) {
        const auto& fl = m_data.flights[i];
        m_flightsTable->setItem(i, 0, new QTableWidgetItem(formatIsoDateRu(fl.flightDateIso)));
        m_flightsTable->setItem(i, 1, new QTableWidgetItem(
            QString::fromStdString(fl.takeoffTime + " – " + fl.landingTime)));
        m_flightsTable->setItem(i, 2, new QTableWidgetItem(formatFlightMinutes(fl.durationMinutes)));
        m_flightsTable->setItem(i, 3, new QTableWidgetItem(QString::fromStdString(fl.operatorName)));
        m_flightsTable->setItem(i, 4, new QTableWidgetItem(QString::fromStdString(fl.batteryInventory)));
        m_flightsTable->setItem(i, 5, new QTableWidgetItem(QString::fromStdString(fl.taskAndArea)));
    }

    // Таблица 2: ТО
    m_maintTable->setRowCount(static_cast<int>(m_data.maintRecords.size()));
    for (int i = 0; i < static_cast<int>(m_data.maintRecords.size()); ++i) {
        const auto& m = m_data.maintRecords[i];
        m_maintTable->setItem(i, 0, new QTableWidgetItem(formatIsoDateRu(m.performedDateIso)));
        m_maintTable->setItem(i, 1, new QTableWidgetItem(QString::fromStdString(m.ruleTitle)));
        m_maintTable->setItem(i, 2, new QTableWidgetItem(formatFlightMinutes(m.flightMinAtMaint)));
        m_maintTable->setItem(i, 3, new QTableWidgetItem(QString::fromStdString(m.performer)));
        m_maintTable->setItem(i, 4, new QTableWidgetItem(QString::fromStdString(m.notes)));
    }

    // Таблица 3: Неисправности
    m_faultsTable->setRowCount(static_cast<int>(m_data.faults.size()));
    for (int i = 0; i < static_cast<int>(m_data.faults.size()); ++i) {
        const auto& f = m_data.faults[i];
        m_faultsTable->setItem(i, 0, new QTableWidgetItem(formatIsoDateRu(f.reportedDateIso)));
        m_faultsTable->setItem(i, 1, new QTableWidgetItem(f.isCritical ? "КРИТИЧНАЯ" : "Некритичная"));
        m_faultsTable->setItem(i, 2, new QTableWidgetItem(QString::fromStdString(f.description)));
        m_faultsTable->setItem(i, 3, new QTableWidgetItem(f.isOpen() ? "Открыта" : ("Устранена " + formatIsoDateRu(f.resolvedDateIso))));
        m_faultsTable->setItem(i, 4, new QTableWidgetItem(QString::fromStdString(f.resolutionWorks)));
    }

    // Таблица 4a: Закреплённые АКБ
    m_akbPreflightTable->setRowCount(static_cast<int>(m_data.assignedBatteries.size()));
    for (int i = 0; i < static_cast<int>(m_data.assignedBatteries.size()); ++i) {
        const auto& b = m_data.assignedBatteries[i];
        m_akbPreflightTable->setItem(i, 0, new QTableWidgetItem(QString::fromStdString(b.inventoryNumber)));
        m_akbPreflightTable->setItem(i, 1, new QTableWidgetItem(QString::fromStdString(b.modelName)));
        m_akbPreflightTable->setItem(i, 2, new QTableWidgetItem(QString("%1 / %2").arg(b.usedCycles).arg(b.maxCycles)));
        m_akbPreflightTable->setItem(i, 3, new QTableWidgetItem(QString("%1 %").arg(b.usageRatio() * 100.0, 0, 'f', 0)));
        m_akbPreflightTable->setItem(i, 4, new QTableWidgetItem(b.isExhausted() ? "Ресурс выработан" : "В эксплуатации"));
    }

    // Таблица 4b: Осмотры F-15
    m_preflightTable->setRowCount(static_cast<int>(m_data.preflightChecks.size()));
    for (int i = 0; i < static_cast<int>(m_data.preflightChecks.size()); ++i) {
        const auto& p = m_data.preflightChecks[i];
        m_preflightTable->setItem(i, 0, new QTableWidgetItem(formatIsoDateRu(p.checkDateIso)));
        auto* resItem = new QTableWidgetItem(p.passed ? "Пройден" : QString("Отказ (%1 крит.)").arg(p.criticalDefectsCount));
        resItem->setForeground(QBrush(p.passed ? QColor("#137333") : QColor("#A50E0E")));
        m_preflightTable->setItem(i, 1, resItem);
        m_preflightTable->setItem(i, 2, new QTableWidgetItem(QString::fromStdString(p.inspectorName)));
        m_preflightTable->setItem(i, 3, new QTableWidgetItem(QString::fromStdString(p.notes)));
    }
}

void UavFormularDialog::onEditPassportClicked() {
    QDialog dlg(this);
    dlg.setWindowTitle("Редактирование паспорта борта — " + QString::fromStdString(m_data.uav.tailNumber));
    auto* lay = new QFormLayout(&dlg);
    auto* tailEdit = new QLineEdit(QString::fromStdString(m_data.uav.tailNumber), &dlg);
    auto* typeCombo = new QComboBox(&dlg);
    for (const auto& t : m_ctx.fleet.loadUavTypes()) {
        typeCombo->addItem(QString::fromStdString(t.name), t.id);
    }
    typeCombo->setCurrentIndex(typeCombo->findData(m_data.uav.uavTypeId));
    auto* serialEdit = new QLineEdit(QString::fromStdString(m_data.uav.serialNumber), &dlg);
    auto* commEdit = new QDateEdit(QDate::fromString(QString::fromStdString(m_data.uav.commissionDateIso), Qt::ISODate), &dlg);
    commEdit->setDisplayFormat("dd.MM.yyyy");
    commEdit->setCalendarPopup(true);
    auto* payloadEdit = new QLineEdit(QString::fromStdString(m_data.uav.standardPayload), &dlg);
    auto* notesEdit = new QLineEdit(QString::fromStdString(m_data.uav.notes), &dlg);

    lay->addRow("Бортовой номер:", tailEdit);
    lay->addRow("Тип БЛА:", typeCombo);
    lay->addRow("Заводской номер:", serialEdit);
    lay->addRow("Дата ввода:", commEdit);
    lay->addRow("Штатная полезная нагрузка:", payloadEdit);
    lay->addRow("Примечание:", notesEdit);

    auto* btns = new QHBoxLayout();
    auto* okBtn = new QPushButton("Сохранить", &dlg);
    auto* cancelBtn = new QPushButton("Отмена", &dlg);
    btns->addStretch();
    btns->addWidget(okBtn);
    btns->addWidget(cancelBtn);
    lay->addRow(btns);

    connect(okBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);

    if (dlg.exec() == QDialog::Accepted) {
        Uav upd = m_data.uav;
        upd.tailNumber = tailEdit->text().trimmed().toStdString();
        upd.uavTypeId = typeCombo->currentData().toInt();
        upd.serialNumber = serialEdit->text().trimmed().toStdString();
        upd.commissionDateIso = commEdit->date().toString(Qt::ISODate).toStdString();
        upd.standardPayload = payloadEdit->text().trimmed().toStdString();
        upd.notes = notesEdit->text().trimmed().toStdString();

        QString err;
        if (!m_ctx.fleet.saveUav(upd, m_actor, nullptr, &err)) {
            QMessageBox::warning(this, "Ошибка сохранения", err);
            return;
        }
        m_modified = true;
        reloadFormular();
    }
}

void UavFormularDialog::onDecommissionClicked() {
    const auto ans = QMessageBox::question(
        this, "Подтверждение списания борта",
        QString("Вы действительно хотите списать борт «%1»?\n\n"
                "Операция необратима (REQ-FUNC-003-2): борт перейдёт в терминальное состояние «Списан» (архив «только чтение»), "
                "вся история полётов, ТО и неисправностей будет сохранена.")
            .arg(QString::fromStdString(m_data.uav.tailNumber)),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (ans != QMessageBox::Yes) return;

    QString err;
    if (!m_ctx.fleet.decommissionUav(m_uavId, m_actor, &err)) {
        QMessageBox::warning(this, "Ошибка списания", err);
        return;
    }
    m_modified = true;
    reloadFormular();
}

void UavFormularDialog::onMaintClicked() {
    MaintenanceDialog dlg(m_ctx, m_actor, m_uavId, this);
    if (dlg.exec() == QDialog::Accepted) {
        m_modified = true;
        reloadFormular();
    }
}

void UavFormularDialog::onFaultClicked() {
    FaultsDialog dlg(m_ctx, m_actor, m_uavId, this);
    dlg.exec();
    m_modified = true;
    reloadFormular();
}

void UavFormularDialog::onExportPdfClicked() {
    QDir().mkpath(m_ctx.exportsDir);
    const QString defName = QDir(m_ctx.exportsDir).filePath(
        QString("formular_%1_%2.pdf")
            .arg(QString::fromStdString(m_data.uav.tailNumber),
                 QDate::currentDate().toString("yyyyMMdd")));
    const QString path = QFileDialog::getSaveFileName(this, "Экспорт формуляра в PDF (A4)", defName, "PDF (*.pdf)");
    if (path.isEmpty()) return;

    QString err;
    if (!m_ctx.reports.exportFormularPdf(m_uavId, path, &err)) {
        QMessageBox::warning(this, "Ошибка экспорта PDF", err);
        return;
    }
    QMessageBox::information(this, "Экспорт завершён", "Формуляр БЛА сохранён в PDF (A4):\n" + path);
}

void UavFormularDialog::onPrintClicked() {
    QString err;
    if (!m_ctx.reports.printFormularA4(m_uavId, this, &err) && !err.isEmpty()) {
        QMessageBox::warning(this, "Ошибка печати", err);
    }
}

} // namespace uavlog
