#include "ui/ReportsDialog.h"

#include <QComboBox>
#include <QDateEdit>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace uavlog {

ReportsDialog::ReportsDialog(AppContext& ctx,
                             const UserAccount& actor,
                             ReportKind initialKind,
                             QWidget* parent)
    : QDialog(parent), m_ctx(ctx), m_actor(actor) {
    setWindowTitle("Отчёты, экспорт CSV/PDF и Акт готовности парка");
    setMinimumSize(1020, 580);

    auto* root = new QVBoxLayout(this);

    auto* grpParams = new QGroupBox("Параметры отчёта", this);
    auto* form = new QFormLayout(grpParams);

    m_kindCombo = new QComboBox(this);
    m_kindCombo->addItem("Налёт по бортам", static_cast<int>(ReportKind::FlightHoursByUav));
    m_kindCombo->addItem("Налёт по операторам", static_cast<int>(ReportKind::FlightHoursByOperator));
    m_kindCombo->addItem("Неисправности", static_cast<int>(ReportKind::FaultsSummary));
    m_kindCombo->addItem("Выполнение ТО", static_cast<int>(ReportKind::MaintenanceExecution));
    m_kindCombo->addItem("Акт готовности парка к полётам (F-15)", static_cast<int>(ReportKind::FleetReadinessAct));
    m_kindCombo->setCurrentIndex(m_kindCombo->findData(static_cast<int>(initialKind)));

    auto* periodRow = new QHBoxLayout();
    m_fromEdit = new QDateEdit(QDate::currentDate().addDays(-90), this);
    m_fromEdit->setDisplayFormat("dd.MM.yyyy");
    m_fromEdit->setCalendarPopup(true);
    m_toEdit = new QDateEdit(QDate::currentDate(), this);
    m_toEdit->setDisplayFormat("dd.MM.yyyy");
    m_toEdit->setCalendarPopup(true);
    periodRow->addWidget(new QLabel("с:", this));
    periodRow->addWidget(m_fromEdit);
    periodRow->addWidget(new QLabel("по:", this));
    periodRow->addWidget(m_toEdit);
    periodRow->addStretch();

    m_filterCombo = new QComboBox(this);
    m_filterCombo->addItem("Все борта / операторы", 0);
    for (const auto& u : m_ctx.fleet.loadUavs(true)) {
        m_filterCombo->addItem("Борт: " + QString::fromStdString(u.tailNumber), u.id);
    }

    form->addRow("Вид отчёта:", m_kindCombo);
    form->addRow("Период:", periodRow);
    form->addRow("Фильтр (борт):", m_filterCombo);
    root->addWidget(grpParams);

    m_titleLabel = new QLabel(this);
    m_titleLabel->setStyleSheet("font-weight:bold;font-size:12px;color:#1D3E5E;");
    root->addWidget(m_titleLabel);

    m_previewTable = new QTableWidget(this);
    m_previewTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_previewTable->setAlternatingRowColors(true);
    m_previewTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    root->addWidget(m_previewTable, 1);

    auto* btns = new QHBoxLayout();
    auto* genBtn = new QPushButton("Сформировать", this);
    genBtn->setDefault(true);
    auto* csvBtn = new QPushButton("Экспорт CSV (UTF-8 BOM)", this);
    auto* pdfBtn = new QPushButton("Экспорт PDF (A4)", this);
    auto* printBtn = new QPushButton("Печать (A4)", this);
    auto* closeBtn = new QPushButton("Закрыть", this);

    btns->addWidget(genBtn);
    btns->addStretch();
    btns->addWidget(csvBtn);
    btns->addWidget(pdfBtn);
    btns->addWidget(printBtn);
    btns->addWidget(closeBtn);
    root->addLayout(btns);

    connect(m_kindCombo, &QComboBox::currentIndexChanged, this, &ReportsDialog::onGenerateClicked);
    connect(genBtn, &QPushButton::clicked, this, &ReportsDialog::onGenerateClicked);
    connect(csvBtn, &QPushButton::clicked, this, &ReportsDialog::onExportCsvClicked);
    connect(pdfBtn, &QPushButton::clicked, this, &ReportsDialog::onExportPdfClicked);
    connect(printBtn, &QPushButton::clicked, this, &ReportsDialog::onPrintClicked);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);

    onGenerateClicked();
}

void ReportsDialog::onGenerateClicked() {
    const auto kind = static_cast<ReportKind>(m_kindCombo->currentData().toInt());
    const int uavFlt = (kind == ReportKind::FlightHoursByOperator) ? 0 : m_filterCombo->currentData().toInt();

    m_currentReport = m_ctx.reports.generateReport(
        kind, m_fromEdit->date(), m_toEdit->date(), uavFlt, 0,
        QString::fromStdString(m_actor.fullName));

    m_titleLabel->setText(m_currentReport.title + " — " + m_currentReport.periodText);

    m_previewTable->clear();
    m_previewTable->setColumnCount(m_currentReport.headers.size());
    m_previewTable->setHorizontalHeaderLabels(m_currentReport.headers);
    for (int c = 0; c < m_currentReport.headers.size(); ++c) {
        m_previewTable->horizontalHeader()->setSectionResizeMode(
            c, (c == m_currentReport.headers.size() - 2 && m_currentReport.headers.size() >= 6)
                   ? QHeaderView::Stretch
                   : QHeaderView::ResizeToContents);
    }
    if (m_currentReport.headers.size() < 6) {
        m_previewTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    }

    const int totalRows = static_cast<int>(m_currentReport.rows.size()) +
                          (m_currentReport.totalsRow.isEmpty() ? 0 : 1);
    m_previewTable->setRowCount(totalRows);

    for (int r = 0; r < static_cast<int>(m_currentReport.rows.size()); ++r) {
        const auto& row = m_currentReport.rows[r];
        for (int c = 0; c < row.size() && c < m_currentReport.headers.size(); ++c) {
            auto* item = new QTableWidgetItem(row[c]);
            if (kind == ReportKind::FleetReadinessAct && c == row.size() - 1) {
                const bool ok = row[c].startsWith("ДОПУЩЕН");
                item->setBackground(QBrush(ok ? QColor("#E6F4EA") : QColor("#FCE8E6")));
                item->setForeground(QBrush(ok ? QColor("#137333") : QColor("#A50E0E")));
            }
            m_previewTable->setItem(r, c, item);
        }
    }

    if (!m_currentReport.totalsRow.isEmpty()) {
        const int tr = static_cast<int>(m_currentReport.rows.size());
        QFont boldFont;
        boldFont.setBold(true);
        for (int c = 0; c < m_currentReport.totalsRow.size(); ++c) {
            auto* item = new QTableWidgetItem(m_currentReport.totalsRow[c]);
            item->setFont(boldFont);
            item->setBackground(QBrush(QColor("#EDF2F7")));
            m_previewTable->setItem(tr, c, item);
        }
    }
}

void ReportsDialog::onExportCsvClicked() {
    QDir().mkpath(m_ctx.exportsDir);
    const QString defName = QDir(m_ctx.exportsDir).filePath(
        QString("uavlog_report_%1.csv").arg(QDate::currentDate().toString("yyyyMMdd")));
    const QString path = QFileDialog::getSaveFileName(this, "Экспорт отчёта в CSV (UTF-8 BOM)", defName, "CSV (*.csv)");
    if (path.isEmpty()) return;

    QString err;
    if (!m_ctx.reports.exportReportCsv(m_currentReport, path, &err)) {
        QMessageBox::warning(this, "Ошибка экспорта CSV", err);
        return;
    }
    QMessageBox::information(this, "CSV сохранён", "Отчёт успешно экспортирован в CSV (UTF-8 BOM, разделитель «;»):\n" + path);
}

void ReportsDialog::onExportPdfClicked() {
    QDir().mkpath(m_ctx.exportsDir);
    const QString defName = QDir(m_ctx.exportsDir).filePath(
        QString("uavlog_report_%1.pdf").arg(QDate::currentDate().toString("yyyyMMdd")));
    const QString path = QFileDialog::getSaveFileName(this, "Экспорт отчёта в PDF (A4)", defName, "PDF (*.pdf)");
    if (path.isEmpty()) return;

    QString err;
    if (!m_ctx.reports.exportReportPdf(m_currentReport, path, &err)) {
        QMessageBox::warning(this, "Ошибка экспорта PDF", err);
        return;
    }
    QMessageBox::information(this, "PDF сохранён", "Отчёт сохранён в PDF формата A4 со строкой подписи:\n" + path);
}

void ReportsDialog::onPrintClicked() {
    QString err;
    if (!m_ctx.reports.printReportA4(m_currentReport, this, &err) && !err.isEmpty()) {
        QMessageBox::warning(this, "Ошибка печати", err);
    }
}

} // namespace uavlog
