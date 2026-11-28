#include "ui/PreflightDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateEdit>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace uavlog {

PreflightDialog::PreflightDialog(AppContext& ctx,
                                 const UserAccount& actor,
                                 int preselectUavId,
                                 const QDate& date,
                                 QWidget* parent)
    : QDialog(parent), m_ctx(ctx), m_actor(actor) {
    setWindowTitle("Предполётный осмотр по контрольной карте (Модуль F-15)");
    setMinimumSize(900, 520);

    auto* root = new QVBoxLayout(this);

    auto* topForm = new QFormLayout();
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

    m_dateEdit = new QDateEdit(date.isValid() ? date : QDate::currentDate(), this);
    m_dateEdit->setDisplayFormat("dd.MM.yyyy");
    m_dateEdit->setCalendarPopup(true);

    m_templateLabel = new QLabel(this);
    m_templateLabel->setStyleSheet("font-weight: bold; color: #1D3E5E;");

    topForm->addRow("Борт БЛА:", m_uavCombo);
    topForm->addRow("Дата осмотра:", m_dateEdit);
    topForm->addRow("Контрольная карта:", m_templateLabel);
    root->addLayout(topForm);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(4);
    m_table->setHorizontalHeaderLabels({"Пункт проверки", "Критичность", "Исправно (нет замечаний)", "Комментарий / замечание"});
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_table->setAlternatingRowColors(true);
    root->addWidget(m_table, 1);

    auto* extraForm = new QFormLayout();
    m_notesEdit = new QLineEdit(this);
    m_notesEdit->setPlaceholderText("Общее заключение по итогам осмотра...");
    m_autoFaultCheck = new QCheckBox("При критичном замечании автоматически создать запись в журнале неисправностей", this);
    m_autoFaultCheck->setChecked(true);
    extraForm->addRow("Заключение:", m_notesEdit);
    extraForm->addRow("", m_autoFaultCheck);
    root->addLayout(extraForm);

    m_verdictBanner = new QLabel(this);
    m_verdictBanner->setWordWrap(true);
    m_verdictBanner->setMargin(8);
    root->addWidget(m_verdictBanner);

    auto* btns = new QHBoxLayout();
    btns->addStretch();
    m_saveBtn = new QPushButton("Сохранить осмотр", this);
    m_saveBtn->setDefault(true);
    auto* cancelBtn = new QPushButton("Отмена", this);
    btns->addWidget(m_saveBtn);
    btns->addWidget(cancelBtn);
    root->addLayout(btns);

    connect(m_uavCombo, &QComboBox::currentIndexChanged, this, &PreflightDialog::loadChecklistForSelectedUav);
    connect(m_table, &QTableWidget::itemChanged, this, &PreflightDialog::updateVerdictBanner);
    connect(m_saveBtn, &QPushButton::clicked, this, &PreflightDialog::onSaveClicked);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    loadChecklistForSelectedUav();
}

void PreflightDialog::loadChecklistForSelectedUav() {
    m_table->blockSignals(true);
    m_table->setRowCount(0);
    m_currentTemplateId = 0;
    m_currentItems.clear();

    const int uavId = m_uavCombo->currentData().toInt();
    const auto tpl = m_ctx.preflight.loadActiveTemplateForUav(uavId);
    if (!tpl.has_value() || tpl->items.empty()) {
        m_templateLabel->setText("Шаблон контрольной карты не настроен (обратитесь к Администратору)");
        m_saveBtn->setEnabled(false);
        m_verdictBanner->setStyleSheet("background:#FFF8E1;border:1px solid #E6A800;color:#795500;");
        m_verdictBanner->setText("Для выбранного типа БЛА отсутствует активный шаблон чек-листа.");
        m_table->blockSignals(false);
        return;
    }

    m_currentTemplateId = tpl->id;
    m_currentItems = tpl->items;
    m_templateLabel->setText(QString::fromStdString(tpl->name) +
                             QString(" (%1 пунктов)").arg(m_currentItems.size()));
    m_saveBtn->setEnabled(true);

    m_table->setRowCount(static_cast<int>(m_currentItems.size()));
    for (int i = 0; i < static_cast<int>(m_currentItems.size()); ++i) {
        const auto& it = m_currentItems[i];
        auto* txtItem = new QTableWidgetItem(QString::fromStdString(it.itemText));
        txtItem->setFlags(txtItem->flags() & ~Qt::ItemIsEditable);
        m_table->setItem(i, 0, txtItem);

        auto* critItem = new QTableWidgetItem(it.isCritical ? "КРИТИЧНЫЙ" : "Обычный");
        critItem->setFlags(critItem->flags() & ~Qt::ItemIsEditable);
        if (it.isCritical) {
            critItem->setForeground(QBrush(QColor("#A50E0E")));
            critItem->setBackground(QBrush(QColor("#FCE8E6")));
        }
        m_table->setItem(i, 1, critItem);

        auto* okItem = new QTableWidgetItem("Исправно");
        okItem->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        okItem->setCheckState(Qt::Checked);
        m_table->setItem(i, 2, okItem);

        auto* commentItem = new QTableWidgetItem("");
        m_table->setItem(i, 3, commentItem);
    }

    m_table->blockSignals(false);
    updateVerdictBanner();
}

void PreflightDialog::updateVerdictBanner() {
    if (m_currentItems.empty()) return;
    int critDefects = 0;
    int minorDefects = 0;
    for (int i = 0; i < m_table->rowCount() && i < static_cast<int>(m_currentItems.size()); ++i) {
        auto* okItem = m_table->item(i, 2);
        const bool isOk = okItem && okItem->checkState() == Qt::Checked;
        if (okItem) {
            m_table->blockSignals(true);
            okItem->setText(isOk ? "Исправно" : "ЗАМЕЧАНИЕ");
            m_table->blockSignals(false);
        }
        if (!isOk) {
            if (m_currentItems[i].isCritical) ++critDefects;
            else ++minorDefects;
        }
    }

    if (critDefects > 0) {
        m_verdictBanner->setStyleSheet(
            "background-color:#FCE8E6;border:1px solid #D93025;color:#A50E0E;font-weight:bold;border-radius:4px;");
        m_verdictBanner->setText(
            QString("ВЫЯВЛЕНЫ КРИТИЧНЫЕ ЗАМЕЧАНИЯ (%1 шт.) — допуск к полёту будет заблокирован, "
                    "борт автоматически перейдёт в состояние «Неисправен» (UavStateMachine).")
                .arg(critDefects));
    } else if (minorDefects > 0) {
        m_verdictBanner->setStyleSheet(
            "background-color:#FFF8E1;border:1px solid #E6A800;color:#795500;font-weight:bold;border-radius:4px;");
        m_verdictBanner->setText(
            QString("Осмотр ПРОЙДЕН с некритичными замечаниями (%1 шт.) — 5-й фактор допуска выполнен.")
                .arg(minorDefects));
    } else {
        m_verdictBanner->setStyleSheet(
            "background-color:#E6F4EA;border:1px solid #1E8E3E;color:#137333;font-weight:bold;border-radius:4px;");
        m_verdictBanner->setText("Осмотр ПРОЙДЕН без замечаний — борт допускается к полёту и Акт готовности.");
    }
}

void PreflightDialog::onSaveClicked() {
    std::vector<PreflightItemResult> results;
    results.reserve(m_currentItems.size());
    for (int i = 0; i < static_cast<int>(m_currentItems.size()); ++i) {
        PreflightItemResult r;
        r.itemId = m_currentItems[i].id;
        r.itemText = m_currentItems[i].itemText;
        r.isCritical = m_currentItems[i].isCritical;
        r.isOk = m_table->item(i, 2)->checkState() == Qt::Checked;
        r.comment = m_table->item(i, 3)->text().trimmed().toStdString();
        if (!r.isOk && r.comment.empty()) {
            QMessageBox::warning(this, "Укажите замечание",
                                 "Для пункта «" + QString::fromStdString(r.itemText) +
                                     "» снята отметка «Исправно» — укажите суть замечания в столбце комментариев.");
            return;
        }
        results.push_back(std::move(r));
    }

    bool passed = false;
    QString err;
    if (!m_ctx.preflight.saveInspection(
            m_uavCombo->currentData().toInt(),
            m_currentTemplateId,
            m_dateEdit->date(),
            results,
            m_notesEdit->text(),
            m_autoFaultCheck->isChecked(),
            m_actor,
            &passed,
            nullptr,
            &err)) {
        QMessageBox::warning(this, "Ошибка сохранения осмотра", err);
        return;
    }

    QMessageBox::information(
        this, "Предполётный осмотр сохранён",
        passed ? "Предполётный осмотр пройден. 5-й фактор допуска к полётам активен на выбранную дату."
               : "Выявлены критичные замечания! Допуск к полёту заблокирован, борт переведён в состояние «Неисправен».");
    accept();
}

} // namespace uavlog
