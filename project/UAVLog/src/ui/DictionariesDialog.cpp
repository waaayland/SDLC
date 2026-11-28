#include "ui/DictionariesDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateEdit>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

namespace uavlog {

DictionariesDialog::DictionariesDialog(AppContext& ctx,
                                       const UserAccount& actor,
                                       int initialTab,
                                       QWidget* parent)
    : QDialog(parent), m_ctx(ctx), m_actor(actor) {
    setWindowTitle("Справочники, реестры и настройки UAVLog 1.0");
    setMinimumSize(980, 620);

    auto* root = new QVBoxLayout(this);
    m_tabs = new QTabWidget(this);

    // ===== Вкладка 0: Типы БЛА и регламенты ТО (F-03) =====
    {
        auto* tab = new QWidget(this);
        auto* lay = new QVBoxLayout(tab);
        auto* split = new QSplitter(Qt::Vertical, tab);

        auto* topBox = new QGroupBox("Типы БЛА", split);
        auto* topLay = new QVBoxLayout(topBox);
        m_typesTable = new QTableWidget(topBox);
        m_typesTable->setColumnCount(4);
        m_typesTable->setHorizontalHeaderLabels({"Наименование", "Производитель", "Типовая нагрузка", "Примечание"});
        m_typesTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        m_typesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_typesTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        topLay->addWidget(m_typesTable);
        auto* tBtns = new QHBoxLayout();
        auto* addTBtn = new QPushButton("Добавить тип БЛА...", topBox);
        tBtns->addWidget(addTBtn);
        tBtns->addStretch();
        topLay->addLayout(tBtns);

        auto* botBox = new QGroupBox("Регламенты ТО для выбранного типа БЛА (REQ-FUNC-002: не менее 5 видов)", split);
        auto* botLay = new QVBoxLayout(botBox);
        m_rulesTable = new QTableWidget(botBox);
        m_rulesTable->setColumnCount(4);
        m_rulesTable->setHorizontalHeaderLabels({"Вид ТО", "Периодичность по налёту, ч", "По календарю, дн.", "Перечень работ"});
        m_rulesTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        m_rulesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_rulesTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        botLay->addWidget(m_rulesTable);
        auto* rBtns = new QHBoxLayout();
        auto* addRBtn = new QPushButton("Добавить вид ТО...", botBox);
        auto* delRBtn = new QPushButton("Удалить вид ТО", botBox);
        rBtns->addWidget(addRBtn);
        rBtns->addWidget(delRBtn);
        rBtns->addStretch();
        botLay->addLayout(rBtns);

        lay->addWidget(split);
        m_tabs->addTab(tab, "Типы БЛА и регламенты ТО");

        connect(m_typesTable, &QTableWidget::currentCellChanged, this, [this]() { reloadTypesAndRules(); });
        connect(addTBtn, &QPushButton::clicked, this, &DictionariesDialog::onAddType);
        connect(addRBtn, &QPushButton::clicked, this, &DictionariesDialog::onAddRule);
        connect(delRBtn, &QPushButton::clicked, this, &DictionariesDialog::onDeleteRule);
    }

    // ===== Вкладка 1: Шаблоны предполётных чек-листов (F-15 / T-15.2) =====
    {
        auto* tab = new QWidget(this);
        auto* lay = new QVBoxLayout(tab);
        auto* split = new QSplitter(Qt::Vertical, tab);

        auto* topBox = new QGroupBox("Шаблоны контрольных карт предполётного осмотра (F-15)", split);
        auto* topLay = new QVBoxLayout(topBox);
        m_tplTable = new QTableWidget(topBox);
        m_tplTable->setColumnCount(4);
        m_tplTable->setHorizontalHeaderLabels({"Тип БЛА", "Наименование шаблона", "Статус", "Пунктов"});
        m_tplTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        m_tplTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_tplTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        topLay->addWidget(m_tplTable);
        auto* tb = new QHBoxLayout();
        auto* addTplBtn = new QPushButton("Создать шаблон...", topBox);
        tb->addWidget(addTplBtn);
        tb->addStretch();
        topLay->addLayout(tb);

        auto* botBox = new QGroupBox("Пункты выбранной контрольной карты", split);
        auto* botLay = new QVBoxLayout(botBox);
        m_itemsTable = new QTableWidget(botBox);
        m_itemsTable->setColumnCount(3);
        m_itemsTable->setHorizontalHeaderLabels({"№", "Текст пункта проверки", "Критичность"});
        m_itemsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        m_itemsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_itemsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        botLay->addWidget(m_itemsTable);
        auto* ib = new QHBoxLayout();
        auto* addItBtn = new QPushButton("Добавить пункт...", botBox);
        auto* delItBtn = new QPushButton("Удалить пункт", botBox);
        ib->addWidget(addItBtn);
        ib->addWidget(delItBtn);
        ib->addStretch();
        botLay->addLayout(ib);

        lay->addWidget(split);
        m_tabs->addTab(tab, "Чек-листы (F-15)");

        connect(m_tplTable, &QTableWidget::currentCellChanged, this, [this]() { reloadChecklists(); });
        connect(addTplBtn, &QPushButton::clicked, this, &DictionariesDialog::onAddChecklistTemplate);
        connect(addItBtn, &QPushButton::clicked, this, &DictionariesDialog::onAddChecklistItem);
        connect(delItBtn, &QPushButton::clicked, this, &DictionariesDialog::onDeleteChecklistItem);
    }

    // ===== Вкладка 2: Реестр БЛА (F-04) =====
    {
        auto* tab = new QWidget(this);
        auto* lay = new QVBoxLayout(tab);
        m_uavsTable = new QTableWidget(tab);
        m_uavsTable->setColumnCount(7);
        m_uavsTable->setHorizontalHeaderLabels({"Борт", "Тип БЛА", "Заводской №", "Дата ввода", "Состояние", "Налёт", "Штатная нагрузка"});
        m_uavsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        m_uavsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_uavsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        lay->addWidget(m_uavsTable, 1);

        auto* btns = new QHBoxLayout();
        auto* addBtn = new QPushButton("Добавить БЛА...", tab);
        auto* editBtn = new QPushButton("Редактировать...", tab);
        auto* decommBtn = new QPushButton("Списать в архив", tab);
        auto* delBtn = new QPushButton("Удалить (только без истории)", tab);
        btns->addWidget(addBtn);
        btns->addWidget(editBtn);
        btns->addWidget(decommBtn);
        btns->addWidget(delBtn);
        btns->addStretch();
        lay->addLayout(btns);
        m_tabs->addTab(tab, "Реестр БЛА");

        connect(addBtn, &QPushButton::clicked, this, [this]() { onAddOrEditUav(false); });
        connect(editBtn, &QPushButton::clicked, this, [this]() { onAddOrEditUav(true); });
        connect(decommBtn, &QPushButton::clicked, this, &DictionariesDialog::onDecommissionUav);
        connect(delBtn, &QPushButton::clicked, this, &DictionariesDialog::onDeleteUav);
    }

    // ===== Вкладка 3: Реестр АКБ (F-05a) =====
    {
        auto* tab = new QWidget(this);
        auto* lay = new QVBoxLayout(tab);
        m_batsTable = new QTableWidget(tab);
        m_batsTable->setColumnCount(6);
        m_batsTable->setHorizontalHeaderLabels({"Инв. номер", "Модель АКБ", "Циклы", "Выработка, %", "Закреплена за бортом", "Статус"});
        m_batsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        m_batsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_batsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        lay->addWidget(m_batsTable, 1);

        auto* btns = new QHBoxLayout();
        auto* addBtn = new QPushButton("Добавить АКБ...", tab);
        auto* editBtn = new QPushButton("Редактировать / закрепить...", tab);
        auto* retBtn = new QPushButton("Списать АКБ", tab);
        btns->addWidget(addBtn);
        btns->addWidget(editBtn);
        btns->addWidget(retBtn);
        btns->addStretch();
        lay->addLayout(btns);
        m_tabs->addTab(tab, "Реестр АКБ");

        connect(addBtn, &QPushButton::clicked, this, [this]() { onAddOrEditBattery(false); });
        connect(editBtn, &QPushButton::clicked, this, [this]() { onAddOrEditBattery(true); });
        connect(retBtn, &QPushButton::clicked, this, &DictionariesDialog::onRetireBattery);
    }

    // ===== Вкладка 4: Операторы и допуски (F-06) =====
    {
        auto* tab = new QWidget(this);
        auto* lay = new QVBoxLayout(tab);
        m_opsTable = new QTableWidget(tab);
        m_opsTable->setColumnCount(6);
        m_opsTable->setHorizontalHeaderLabels({"ФИО", "Должность", "Документ", "Допуск до", "Допущенные типы БЛА", "Статус допуска"});
        m_opsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        m_opsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_opsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        lay->addWidget(m_opsTable, 1);

        auto* btns = new QHBoxLayout();
        auto* addBtn = new QPushButton("Добавить оператора...", tab);
        auto* editBtn = new QPushButton("Редактировать / продлить допуск...", tab);
        btns->addWidget(addBtn);
        btns->addWidget(editBtn);
        btns->addStretch();
        lay->addLayout(btns);
        m_tabs->addTab(tab, "Операторы и допуски");

        connect(addBtn, &QPushButton::clicked, this, [this]() { onAddOrEditOperator(false); });
        connect(editBtn, &QPushButton::clicked, this, [this]() { onAddOrEditOperator(true); });
    }

    // ===== Вкладка 5: Пользователи и пороги предупреждений (F-02, F-09) =====
    {
        auto* tab = new QWidget(this);
        auto* lay = new QVBoxLayout(tab);

        auto* uBox = new QGroupBox("Учётные записи пользователей и роли RBAC (REQ-SEC-001/002)", tab);
        auto* uLay = new QVBoxLayout(uBox);
        m_usersTable = new QTableWidget(uBox);
        m_usersTable->setColumnCount(4);
        m_usersTable->setHorizontalHeaderLabels({"Логин", "ФИО", "Роль RBAC", "Активна"});
        m_usersTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        m_usersTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_usersTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        uLay->addWidget(m_usersTable);
        auto* ub = new QHBoxLayout();
        auto* addUBtn = new QPushButton("Добавить пользователя...", uBox);
        auto* togUBtn = new QPushButton("Включить / отключить", uBox);
        ub->addWidget(addUBtn);
        ub->addWidget(togUBtn);
        ub->addStretch();
        uLay->addLayout(ub);
        lay->addWidget(uBox, 1);

        auto* sBox = new QGroupBox("Настраиваемые пороги панели предупреждений (REQ-FUNC-009-2, REQ-REUSE-001)", tab);
        auto* sForm = new QFormLayout(sBox);
        m_maintPctSpin = new QSpinBox(sBox);
        m_maintPctSpin->setRange(1, 50);
        m_maintPctSpin->setSuffix(" % периодичности налёта до ТО");
        m_maintDaysSpin = new QSpinBox(sBox);
        m_maintDaysSpin->setRange(1, 90);
        m_maintDaysSpin->setSuffix(" дней до ТО по календарю");
        m_permitDaysSpin = new QSpinBox(sBox);
        m_permitDaysSpin->setRange(1, 90);
        m_permitDaysSpin->setSuffix(" дней до окончания допуска оператора");
        m_batPctSpin = new QSpinBox(sBox);
        m_batPctSpin->setRange(50, 99);
        m_batPctSpin->setSuffix(" % выработки ресурса АКБ");

        auto* saveThBtn = new QPushButton("Сохранить пороги в БД", sBox);
        sForm->addRow("Предупреждение ТО (налёт):", m_maintPctSpin);
        sForm->addRow("Предупреждение ТО (календарь):", m_maintDaysSpin);
        sForm->addRow("Предупреждение допуска:", m_permitDaysSpin);
        sForm->addRow("Предупреждение ресурса АКБ:", m_batPctSpin);
        sForm->addRow("", saveThBtn);
        lay->addWidget(sBox);

        m_tabs->addTab(tab, "Пользователи и настройки");

        connect(addUBtn, &QPushButton::clicked, this, &DictionariesDialog::onAddUser);
        connect(togUBtn, &QPushButton::clicked, this, &DictionariesDialog::onToggleUserActive);
        connect(saveThBtn, &QPushButton::clicked, this, &DictionariesDialog::onSaveThresholds);
    }

    root->addWidget(m_tabs, 1);

    auto* botBtns = new QHBoxLayout();
    botBtns->addStretch();
    auto* closeBtn = new QPushButton("Закрыть", this);
    botBtns->addWidget(closeBtn);
    root->addLayout(botBtns);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);

    reloadTypesAndRules();
    reloadChecklists();
    reloadUavs();
    reloadBatteries();
    reloadOperators();
    reloadUsersAndSettings();

    if (initialTab >= 0 && initialTab < m_tabs->count()) {
        m_tabs->setCurrentIndex(initialTab);
    }
}

void DictionariesDialog::reloadTypesAndRules() {
    const int prevRow = m_typesTable->currentRow();
    m_typesTable->blockSignals(true);
    m_types = m_ctx.fleet.loadUavTypes();
    m_typesTable->setRowCount(static_cast<int>(m_types.size()));
    for (int i = 0; i < static_cast<int>(m_types.size()); ++i) {
        m_typesTable->setItem(i, 0, new QTableWidgetItem(QString::fromStdString(m_types[i].name)));
        m_typesTable->setItem(i, 1, new QTableWidgetItem(QString::fromStdString(m_types[i].manufacturer)));
        m_typesTable->setItem(i, 2, new QTableWidgetItem(QString::fromStdString(m_types[i].defaultPayload)));
        m_typesTable->setItem(i, 3, new QTableWidgetItem(QString::fromStdString(m_types[i].notes)));
    }
    const int selRow = (prevRow >= 0 && prevRow < static_cast<int>(m_types.size())) ? prevRow : (m_types.empty() ? -1 : 0);
    if (selRow >= 0) m_typesTable->setCurrentCell(selRow, 0);
    m_typesTable->blockSignals(false);

    m_rules.clear();
    m_rulesTable->setRowCount(0);
    if (selRow < 0) return;

    m_rules = m_ctx.fleet.loadMaintenanceRules(m_types[selRow].id);
    m_rulesTable->setRowCount(static_cast<int>(m_rules.size()));
    for (int i = 0; i < static_cast<int>(m_rules.size()); ++i) {
        const auto& r = m_rules[i];
        m_rulesTable->setItem(i, 0, new QTableWidgetItem(QString::fromStdString(r.title)));
        m_rulesTable->setItem(i, 1, new QTableWidgetItem(r.intervalHours.has_value() ? QString::number(*r.intervalHours, 'f', 1) : "—"));
        m_rulesTable->setItem(i, 2, new QTableWidgetItem(r.intervalDays.has_value() ? QString::number(*r.intervalDays) : "—"));
        QStringList tList;
        for (const auto& t : r.tasks) tList << QString::fromStdString(t);
        m_rulesTable->setItem(i, 3, new QTableWidgetItem(tList.join("; ")));
    }
}

void DictionariesDialog::onAddType() {
    QDialog dlg(this);
    dlg.setWindowTitle("Новый тип БЛА");
    auto* lay = new QFormLayout(&dlg);
    auto* nameE = new QLineEdit(&dlg);
    auto* mfrE = new QLineEdit(&dlg);
    auto* plE = new QLineEdit(&dlg);
    auto* ntE = new QLineEdit(&dlg);
    lay->addRow("Наименование:", nameE);
    lay->addRow("Производитель:", mfrE);
    lay->addRow("Штатная нагрузка:", plE);
    lay->addRow("Примечание:", ntE);
    auto* btns = new QHBoxLayout();
    auto* ok = new QPushButton("Сохранить", &dlg);
    auto* cancel = new QPushButton("Отмена", &dlg);
    btns->addStretch(); btns->addWidget(ok); btns->addWidget(cancel);
    lay->addRow(btns);
    connect(ok, &QPushButton::clicked, &dlg, &QDialog::accept);
    connect(cancel, &QPushButton::clicked, &dlg, &QDialog::reject);

    if (dlg.exec() == QDialog::Accepted) {
        UavType t;
        t.name = nameE->text().trimmed().toStdString();
        t.manufacturer = mfrE->text().trimmed().toStdString();
        t.defaultPayload = plE->text().trimmed().toStdString();
        t.notes = ntE->text().trimmed().toStdString();
        QString err;
        if (!m_ctx.fleet.saveUavType(t, m_actor, nullptr, &err)) {
            QMessageBox::warning(this, "Ошибка", err);
            return;
        }
        reloadTypesAndRules();
    }
}

void DictionariesDialog::onAddRule() {
    const int row = m_typesTable->currentRow();
    if (row < 0 || row >= static_cast<int>(m_types.size())) return;

    QDialog dlg(this);
    dlg.setWindowTitle("Новый регламент ТО для " + QString::fromStdString(m_types[row].name));
    auto* lay = new QFormLayout(&dlg);
    auto* titleE = new QLineEdit(&dlg);
    auto* hoursSpin = new QDoubleSpinBox(&dlg);
    hoursSpin->setRange(0.0, 5000.0);
    hoursSpin->setSpecialValueText("Не задано");
    auto* daysSpin = new QSpinBox(&dlg);
    daysSpin->setRange(0, 3650);
    daysSpin->setSpecialValueText("Не задано");
    auto* tasksEdit = new QPlainTextEdit(&dlg);
    tasksEdit->setPlaceholderText("По одному пункту работ на строку...");
    tasksEdit->setMaximumHeight(110);

    lay->addRow("Наименование вида ТО:", titleE);
    lay->addRow("Периодичность по налёту (ч):", hoursSpin);
    lay->addRow("Периодичность по календарю (дни):", daysSpin);
    lay->addRow("Перечень работ:", tasksEdit);

    auto* btns = new QHBoxLayout();
    auto* ok = new QPushButton("Сохранить", &dlg);
    auto* cancel = new QPushButton("Отмена", &dlg);
    btns->addStretch(); btns->addWidget(ok); btns->addWidget(cancel);
    lay->addRow(btns);
    connect(ok, &QPushButton::clicked, &dlg, &QDialog::accept);
    connect(cancel, &QPushButton::clicked, &dlg, &QDialog::reject);

    if (dlg.exec() == QDialog::Accepted) {
        MaintenanceRule r;
        r.uavTypeId = m_types[row].id;
        r.title = titleE->text().trimmed().toStdString();
        if (hoursSpin->value() > 0.0) r.intervalHours = hoursSpin->value();
        if (daysSpin->value() > 0) r.intervalDays = daysSpin->value();
        for (const auto& line : tasksEdit->toPlainText().split('\n', Qt::SkipEmptyParts)) {
            if (!line.trimmed().isEmpty()) r.tasks.push_back(line.trimmed().toStdString());
        }
        QString err;
        if (!m_ctx.fleet.saveMaintenanceRule(r, m_actor, nullptr, &err)) {
            QMessageBox::warning(this, "Ошибка валидации регламента", err);
            return;
        }
        reloadTypesAndRules();
    }
}

void DictionariesDialog::onDeleteRule() {
    const int row = m_rulesTable->currentRow();
    if (row < 0 || row >= static_cast<int>(m_rules.size())) return;
    QString err;
    if (!m_ctx.fleet.deleteMaintenanceRule(m_rules[row].id, m_actor, &err)) {
        QMessageBox::warning(this, "Удаление невозможно", err);
        return;
    }
    reloadTypesAndRules();
}

void DictionariesDialog::reloadChecklists() {
    const int prevRow = m_tplTable->currentRow();
    m_tplTable->blockSignals(true);
    m_templates = m_ctx.preflight.loadTemplates();
    m_tplTable->setRowCount(static_cast<int>(m_templates.size()));
    for (int i = 0; i < static_cast<int>(m_templates.size()); ++i) {
        m_tplTable->setItem(i, 0, new QTableWidgetItem(QString::fromStdString(m_templates[i].uavTypeName)));
        m_tplTable->setItem(i, 1, new QTableWidgetItem(QString::fromStdString(m_templates[i].name)));
        m_tplTable->setItem(i, 2, new QTableWidgetItem(m_templates[i].active ? "Активен" : "Архив"));
        m_tplTable->setItem(i, 3, new QTableWidgetItem(QString::number(m_templates[i].items.size())));
    }
    const int selRow = (prevRow >= 0 && prevRow < static_cast<int>(m_templates.size())) ? prevRow : (m_templates.empty() ? -1 : 0);
    if (selRow >= 0) m_tplTable->setCurrentCell(selRow, 0);
    m_tplTable->blockSignals(false);

    m_itemsTable->setRowCount(0);
    if (selRow < 0) return;

    const auto& items = m_templates[selRow].items;
    m_itemsTable->setRowCount(static_cast<int>(items.size()));
    for (int i = 0; i < static_cast<int>(items.size()); ++i) {
        m_itemsTable->setItem(i, 0, new QTableWidgetItem(QString::number(i + 1)));
        m_itemsTable->setItem(i, 1, new QTableWidgetItem(QString::fromStdString(items[i].itemText)));
        auto* cItem = new QTableWidgetItem(items[i].isCritical ? "КРИТИЧНЫЙ" : "Обычный");
        if (items[i].isCritical) cItem->setForeground(QBrush(QColor("#A50E0E")));
        m_itemsTable->setItem(i, 2, cItem);
    }
}

void DictionariesDialog::onAddChecklistTemplate() {
    QDialog dlg(this);
    dlg.setWindowTitle("Новый шаблон контрольной карты (F-15)");
    auto* lay = new QFormLayout(&dlg);
    auto* typeCombo = new QComboBox(&dlg);
    for (const auto& t : m_ctx.fleet.loadUavTypes()) {
        typeCombo->addItem(QString::fromStdString(t.name), t.id);
    }
    auto* nameE = new QLineEdit(&dlg);
    lay->addRow("Тип БЛА:", typeCombo);
    lay->addRow("Наименование шаблона:", nameE);
    auto* btns = new QHBoxLayout();
    auto* ok = new QPushButton("Создать", &dlg);
    auto* cancel = new QPushButton("Отмена", &dlg);
    btns->addStretch(); btns->addWidget(ok); btns->addWidget(cancel);
    lay->addRow(btns);
    connect(ok, &QPushButton::clicked, &dlg, &QDialog::accept);
    connect(cancel, &QPushButton::clicked, &dlg, &QDialog::reject);

    if (dlg.exec() == QDialog::Accepted) {
        ChecklistTemplate tpl;
        tpl.uavTypeId = typeCombo->currentData().toInt();
        tpl.name = nameE->text().trimmed().toStdString();
        tpl.active = true;
        QString err;
        if (!m_ctx.preflight.saveTemplate(tpl, m_actor, nullptr, &err)) {
            QMessageBox::warning(this, "Ошибка", err);
            return;
        }
        reloadChecklists();
    }
}

void DictionariesDialog::onAddChecklistItem() {
    const int row = m_tplTable->currentRow();
    if (row < 0 || row >= static_cast<int>(m_templates.size())) return;

    QDialog dlg(this);
    dlg.setWindowTitle("Новый пункт контрольной карты");
    auto* lay = new QFormLayout(&dlg);
    auto* txtE = new QLineEdit(&dlg);
    auto* critChk = new QCheckBox("Критичный пункт (замечание блокирует допуск к полёту)", &dlg);
    critChk->setChecked(true);
    lay->addRow("Текст проверки:", txtE);
    lay->addRow("", critChk);
    auto* btns = new QHBoxLayout();
    auto* ok = new QPushButton("Добавить", &dlg);
    auto* cancel = new QPushButton("Отмена", &dlg);
    btns->addStretch(); btns->addWidget(ok); btns->addWidget(cancel);
    lay->addRow(btns);
    connect(ok, &QPushButton::clicked, &dlg, &QDialog::accept);
    connect(cancel, &QPushButton::clicked, &dlg, &QDialog::reject);

    if (dlg.exec() == QDialog::Accepted) {
        ChecklistItem item;
        item.templateId = m_templates[row].id;
        item.itemText = txtE->text().trimmed().toStdString();
        item.isCritical = critChk->isChecked();
        item.sortOrder = static_cast<int>(m_templates[row].items.size()) + 1;
        QString err;
        if (!m_ctx.preflight.saveChecklistItem(item, m_actor, nullptr, &err)) {
            QMessageBox::warning(this, "Ошибка", err);
            return;
        }
        reloadChecklists();
    }
}

void DictionariesDialog::onDeleteChecklistItem() {
    const int tRow = m_tplTable->currentRow();
    const int iRow = m_itemsTable->currentRow();
    if (tRow < 0 || tRow >= static_cast<int>(m_templates.size())) return;
    if (iRow < 0 || iRow >= static_cast<int>(m_templates[tRow].items.size())) return;

    QString err;
    if (!m_ctx.preflight.deleteChecklistItem(m_templates[tRow].items[iRow].id, m_actor, &err)) {
        QMessageBox::warning(this, "Удаление невозможно", err);
        return;
    }
    reloadChecklists();
}

void DictionariesDialog::reloadUavs() {
    m_uavs = m_ctx.fleet.loadUavs(true);
    m_uavsTable->setRowCount(static_cast<int>(m_uavs.size()));
    for (int i = 0; i < static_cast<int>(m_uavs.size()); ++i) {
        const auto& u = m_uavs[i];
        m_uavsTable->setItem(i, 0, new QTableWidgetItem(QString::fromStdString(u.tailNumber)));
        m_uavsTable->setItem(i, 1, new QTableWidgetItem(QString::fromStdString(u.typeName)));
        m_uavsTable->setItem(i, 2, new QTableWidgetItem(QString::fromStdString(u.serialNumber)));
        m_uavsTable->setItem(i, 3, new QTableWidgetItem(formatIsoDateRu(u.commissionDateIso)));
        m_uavsTable->setItem(i, 4, new QTableWidgetItem(QString::fromStdString(toRussianStatus(u.status))));
        m_uavsTable->setItem(i, 5, new QTableWidgetItem(formatFlightMinutes(u.totalFlightMin)));
        m_uavsTable->setItem(i, 6, new QTableWidgetItem(QString::fromStdString(u.standardPayload)));
    }
}

void DictionariesDialog::onAddOrEditUav(bool isEdit) {
    Uav cur;
    if (isEdit) {
        const int row = m_uavsTable->currentRow();
        if (row < 0 || row >= static_cast<int>(m_uavs.size())) return;
        cur = m_uavs[row];
    } else {
        cur.commissionDateIso = QDate::currentDate().toString(Qt::ISODate).toStdString();
    }

    QDialog dlg(this);
    dlg.setWindowTitle(isEdit ? "Редактировать борт БЛА" : "Добавить новый борт БЛА");
    auto* lay = new QFormLayout(&dlg);
    auto* tailE = new QLineEdit(QString::fromStdString(cur.tailNumber), &dlg);
    auto* typeC = new QComboBox(&dlg);
    for (const auto& t : m_ctx.fleet.loadUavTypes()) {
        typeC->addItem(QString::fromStdString(t.name), t.id);
    }
    if (cur.uavTypeId > 0) typeC->setCurrentIndex(typeC->findData(cur.uavTypeId));
    auto* serE = new QLineEdit(QString::fromStdString(cur.serialNumber), &dlg);
    auto* dtE = new QDateEdit(QDate::fromString(QString::fromStdString(cur.commissionDateIso), Qt::ISODate), &dlg);
    dtE->setDisplayFormat("dd.MM.yyyy");
    dtE->setCalendarPopup(true);
    auto* plE = new QLineEdit(QString::fromStdString(cur.standardPayload), &dlg);
    auto* ntE = new QLineEdit(QString::fromStdString(cur.notes), &dlg);

    lay->addRow("Бортовой номер (уникальный):", tailE);
    lay->addRow("Тип БЛА:", typeC);
    lay->addRow("Заводской номер:", serE);
    lay->addRow("Дата ввода:", dtE);
    lay->addRow("Штатная полезная нагрузка:", plE);
    lay->addRow("Примечание:", ntE);

    auto* btns = new QHBoxLayout();
    auto* ok = new QPushButton("Сохранить", &dlg);
    auto* cancel = new QPushButton("Отмена", &dlg);
    btns->addStretch(); btns->addWidget(ok); btns->addWidget(cancel);
    lay->addRow(btns);
    connect(ok, &QPushButton::clicked, &dlg, &QDialog::accept);
    connect(cancel, &QPushButton::clicked, &dlg, &QDialog::reject);

    if (dlg.exec() == QDialog::Accepted) {
        cur.tailNumber = tailE->text().trimmed().toStdString();
        cur.uavTypeId = typeC->currentData().toInt();
        cur.serialNumber = serE->text().trimmed().toStdString();
        cur.commissionDateIso = dtE->date().toString(Qt::ISODate).toStdString();
        cur.standardPayload = plE->text().trimmed().toStdString();
        cur.notes = ntE->text().trimmed().toStdString();
        QString err;
        if (!m_ctx.fleet.saveUav(cur, m_actor, nullptr, &err)) {
            QMessageBox::warning(this, "Ошибка сохранения борта", err);
            return;
        }
        reloadUavs();
    }
}

void DictionariesDialog::onDecommissionUav() {
    const int row = m_uavsTable->currentRow();
    if (row < 0 || row >= static_cast<int>(m_uavs.size())) return;
    if (QMessageBox::question(this, "Списание борта",
                              "Списать борт «" + QString::fromStdString(m_uavs[row].tailNumber) + "» в архив?",
                              QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) {
        return;
    }
    QString err;
    if (!m_ctx.fleet.decommissionUav(m_uavs[row].id, m_actor, &err)) {
        QMessageBox::warning(this, "Ошибка списания", err);
        return;
    }
    reloadUavs();
}

void DictionariesDialog::onDeleteUav() {
    const int row = m_uavsTable->currentRow();
    if (row < 0 || row >= static_cast<int>(m_uavs.size())) return;
    QString err;
    if (!m_ctx.fleet.deleteUav(m_uavs[row].id, m_actor, &err)) {
        QMessageBox::warning(this, "Удаление запрещено", err);
        return;
    }
    reloadUavs();
}

void DictionariesDialog::reloadBatteries() {
    m_batteries = m_ctx.fleet.loadBatteries(false);
    const auto th = m_ctx.fleet.loadAlertThresholds();
    m_batsTable->setRowCount(static_cast<int>(m_batteries.size()));
    for (int i = 0; i < static_cast<int>(m_batteries.size()); ++i) {
        const auto& b = m_batteries[i];
        m_batsTable->setItem(i, 0, new QTableWidgetItem(QString::fromStdString(b.inventoryNumber)));
        m_batsTable->setItem(i, 1, new QTableWidgetItem(QString::fromStdString(b.modelName)));
        m_batsTable->setItem(i, 2, new QTableWidgetItem(QString("%1 / %2").arg(b.usedCycles).arg(b.maxCycles)));
        const int pct = static_cast<int>(std::round(b.usageRatio() * 100.0));
        m_batsTable->setItem(i, 3, new QTableWidgetItem(QString("%1 %").arg(pct)));
        m_batsTable->setItem(i, 4, new QTableWidgetItem(b.assignedUavTail.empty() ? "Свободна" : QString::fromStdString(b.assignedUavTail)));

        QString st = "В эксплуатации";
        auto* stItem = new QTableWidgetItem();
        if (b.retired) {
            st = "Списана";
        } else if (b.isExhausted()) {
            st = "РЕСУРС ВЫРАБОТАН (заблокирована)";
            stItem->setBackground(QBrush(QColor("#FCE8E6")));
            stItem->setForeground(QBrush(QColor("#A50E0E")));
        } else if (b.usageRatio() >= th.batteryWarnRatio) {
            st = "Внимание: >= 90 % ресурса";
            stItem->setBackground(QBrush(QColor("#FFF8E1")));
            stItem->setForeground(QBrush(QColor("#795500")));
        } else {
            stItem->setBackground(QBrush(QColor("#E6F4EA")));
            stItem->setForeground(QBrush(QColor("#137333")));
        }
        stItem->setText(st);
        m_batsTable->setItem(i, 5, stItem);
    }
}

void DictionariesDialog::onAddOrEditBattery(bool isEdit) {
    Battery cur;
    if (isEdit) {
        const int row = m_batsTable->currentRow();
        if (row < 0 || row >= static_cast<int>(m_batteries.size())) return;
        cur = m_batteries[row];
    } else {
        cur.commissionDateIso = QDate::currentDate().toString(Qt::ISODate).toStdString();
    }

    QDialog dlg(this);
    dlg.setWindowTitle(isEdit ? "Редактировать АКБ" : "Добавить АКБ");
    auto* lay = new QFormLayout(&dlg);
    auto* invE = new QLineEdit(QString::fromStdString(cur.inventoryNumber), &dlg);
    auto* modE = new QLineEdit(QString::fromStdString(cur.modelName), &dlg);
    auto* maxS = new QSpinBox(&dlg);
    maxS->setRange(1, 10000);
    maxS->setValue(cur.maxCycles);
    auto* uavC = new QComboBox(&dlg);
    uavC->addItem("Не закреплена", 0);
    for (const auto& u : m_ctx.fleet.loadUavs(false)) {
        uavC->addItem(QString::fromStdString(u.tailNumber), u.id);
    }
    if (cur.assignedUavId.has_value()) uavC->setCurrentIndex(uavC->findData(*cur.assignedUavId));

    lay->addRow("Инвентарный номер:", invE);
    lay->addRow("Модель / тип АКБ:", modE);
    lay->addRow("Лимит циклов:", maxS);
    lay->addRow("Закрепить за бортом:", uavC);

    auto* btns = new QHBoxLayout();
    auto* ok = new QPushButton("Сохранить", &dlg);
    auto* cancel = new QPushButton("Отмена", &dlg);
    btns->addStretch(); btns->addWidget(ok); btns->addWidget(cancel);
    lay->addRow(btns);
    connect(ok, &QPushButton::clicked, &dlg, &QDialog::accept);
    connect(cancel, &QPushButton::clicked, &dlg, &QDialog::reject);

    if (dlg.exec() == QDialog::Accepted) {
        cur.inventoryNumber = invE->text().trimmed().toStdString();
        cur.modelName = modE->text().trimmed().toStdString();
        cur.maxCycles = maxS->value();
        const int uId = uavC->currentData().toInt();
        cur.assignedUavId = (uId > 0) ? std::optional<int>(uId) : std::nullopt;
        QString err;
        if (!m_ctx.fleet.saveBattery(cur, m_actor, nullptr, &err)) {
            QMessageBox::warning(this, "Ошибка сохранения АКБ", err);
            return;
        }
        reloadBatteries();
    }
}

void DictionariesDialog::onRetireBattery() {
    const int row = m_batsTable->currentRow();
    if (row < 0 || row >= static_cast<int>(m_batteries.size())) return;
    QString err;
    if (!m_ctx.fleet.retireBattery(m_batteries[row].id, m_actor, &err)) {
        QMessageBox::warning(this, "Ошибка", err);
        return;
    }
    reloadBatteries();
}

void DictionariesDialog::reloadOperators() {
    m_operators = m_ctx.fleet.loadOperators(false);
    const auto allTypes = m_ctx.fleet.loadUavTypes();
    const int todayNum = static_cast<int>(QDate::currentDate().toJulianDay());
    const auto th = m_ctx.fleet.loadAlertThresholds();

    m_opsTable->setRowCount(static_cast<int>(m_operators.size()));
    for (int i = 0; i < static_cast<int>(m_operators.size()); ++i) {
        const auto& op = m_operators[i];
        m_opsTable->setItem(i, 0, new QTableWidgetItem(QString::fromStdString(op.fullName)));
        m_opsTable->setItem(i, 1, new QTableWidgetItem(QString::fromStdString(op.position)));
        m_opsTable->setItem(i, 2, new QTableWidgetItem(QString::fromStdString(op.permitNumber)));
        m_opsTable->setItem(i, 3, new QTableWidgetItem(formatIsoDateRu(op.permitExpiryIso)));

        QStringList tNames;
        for (int tid : op.allowedUavTypeIds) {
            for (const auto& t : allTypes) {
                if (t.id == tid) tNames << QString::fromStdString(t.name);
            }
        }
        m_opsTable->setItem(i, 4, new QTableWidgetItem(tNames.join(", ")));

        const int daysLeft = op.permitExpiryDayNumber - todayNum;
        auto* stItem = new QTableWidgetItem();
        if (daysLeft < 0) {
            stItem->setText("ДОПУСК ИСТЁК");
            stItem->setBackground(QBrush(QColor("#FCE8E6")));
            stItem->setForeground(QBrush(QColor("#A50E0E")));
        } else if (daysLeft <= th.permitDaysWarn) {
            stItem->setText(QString("Истекает через %1 дн.").arg(daysLeft));
            stItem->setBackground(QBrush(QColor("#FFF8E1")));
            stItem->setForeground(QBrush(QColor("#795500")));
        } else {
            stItem->setText(QString("Действует (%1 дн.)").arg(daysLeft));
            stItem->setBackground(QBrush(QColor("#E6F4EA")));
            stItem->setForeground(QBrush(QColor("#137333")));
        }
        m_opsTable->setItem(i, 5, stItem);
    }
}

void DictionariesDialog::onAddOrEditOperator(bool isEdit) {
    OperatorInfo cur;
    if (isEdit) {
        const int row = m_opsTable->currentRow();
        if (row < 0 || row >= static_cast<int>(m_operators.size())) return;
        cur = m_operators[row];
    } else {
        cur.permitExpiryIso = QDate::currentDate().addYears(1).toString(Qt::ISODate).toStdString();
    }

    QDialog dlg(this);
    dlg.setWindowTitle(isEdit ? "Редактировать оператора" : "Добавить оператора БЛА");
    auto* lay = new QFormLayout(&dlg);
    auto* nameE = new QLineEdit(QString::fromStdString(cur.fullName), &dlg);
    auto* posE = new QLineEdit(QString::fromStdString(cur.position), &dlg);
    auto* docE = new QLineEdit(QString::fromStdString(cur.permitNumber), &dlg);
    auto* expE = new QDateEdit(QDate::fromString(QString::fromStdString(cur.permitExpiryIso), Qt::ISODate), &dlg);
    expE->setDisplayFormat("dd.MM.yyyy");
    expE->setCalendarPopup(true);

    auto* typesList = new QListWidget(&dlg);
    typesList->setMaximumHeight(110);
    for (const auto& t : m_ctx.fleet.loadUavTypes()) {
        auto* it = new QListWidgetItem(QString::fromStdString(t.name), typesList);
        it->setData(Qt::UserRole, t.id);
        it->setFlags(it->flags() | Qt::ItemIsUserCheckable);
        const bool checked = std::find(cur.allowedUavTypeIds.begin(), cur.allowedUavTypeIds.end(), t.id) != cur.allowedUavTypeIds.end();
        it->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
    }

    lay->addRow("ФИО:", nameE);
    lay->addRow("Должность:", posE);
    lay->addRow("№ документа о допуске:", docE);
    lay->addRow("Дата окончания допуска:", expE);
    lay->addRow("Допущенные типы БЛА:", typesList);

    auto* btns = new QHBoxLayout();
    auto* ok = new QPushButton("Сохранить", &dlg);
    auto* cancel = new QPushButton("Отмена", &dlg);
    btns->addStretch(); btns->addWidget(ok); btns->addWidget(cancel);
    lay->addRow(btns);
    connect(ok, &QPushButton::clicked, &dlg, &QDialog::accept);
    connect(cancel, &QPushButton::clicked, &dlg, &QDialog::reject);

    if (dlg.exec() == QDialog::Accepted) {
        cur.fullName = nameE->text().trimmed().toStdString();
        cur.position = posE->text().trimmed().toStdString();
        cur.permitNumber = docE->text().trimmed().toStdString();
        cur.permitExpiryIso = expE->date().toString(Qt::ISODate).toStdString();
        cur.allowedUavTypeIds.clear();
        for (int i = 0; i < typesList->count(); ++i) {
            if (typesList->item(i)->checkState() == Qt::Checked) {
                cur.allowedUavTypeIds.push_back(typesList->item(i)->data(Qt::UserRole).toInt());
            }
        }
        QString err;
        if (!m_ctx.fleet.saveOperator(cur, m_actor, nullptr, &err)) {
            QMessageBox::warning(this, "Ошибка сохранения оператора", err);
            return;
        }
        reloadOperators();
    }
}

void DictionariesDialog::reloadUsersAndSettings() {
    m_users = m_ctx.auth.listUsers(m_actor);
    m_usersTable->setRowCount(static_cast<int>(m_users.size()));
    for (int i = 0; i < static_cast<int>(m_users.size()); ++i) {
        m_usersTable->setItem(i, 0, new QTableWidgetItem(QString::fromStdString(m_users[i].username)));
        m_usersTable->setItem(i, 1, new QTableWidgetItem(QString::fromStdString(m_users[i].fullName)));
        m_usersTable->setItem(i, 2, new QTableWidgetItem(QString::fromStdString(toRussianRole(m_users[i].role))));
        m_usersTable->setItem(i, 3, new QTableWidgetItem(m_users[i].active ? "Активна" : "Отключена"));
    }

    const auto th = m_ctx.fleet.loadAlertThresholds();
    m_maintPctSpin->setValue(static_cast<int>(std::round(th.maintHoursWarnRatio * 100.0)));
    m_maintDaysSpin->setValue(th.maintDaysWarn);
    m_permitDaysSpin->setValue(th.permitDaysWarn);
    m_batPctSpin->setValue(static_cast<int>(std::round(th.batteryWarnRatio * 100.0)));
}

void DictionariesDialog::onAddUser() {
    QDialog dlg(this);
    dlg.setWindowTitle("Новая учётная запись");
    auto* lay = new QFormLayout(&dlg);
    auto* logE = new QLineEdit(&dlg);
    auto* nameE = new QLineEdit(&dlg);
    auto* roleC = new QComboBox(&dlg);
    roleC->addItem("Администратор", static_cast<int>(UserRole::Administrator));
    roleC->addItem("Техник", static_cast<int>(UserRole::Technician));
    roleC->addItem("Оператор", static_cast<int>(UserRole::Operator));
    roleC->addItem("Наблюдатель", static_cast<int>(UserRole::Observer));
    auto* passE = new QLineEdit(&dlg);
    passE->setEchoMode(QLineEdit::Password);
    passE->setPlaceholderText("Минимум 8 символов (REQ-SEC-001)");

    lay->addRow("Логин:", logE);
    lay->addRow("ФИО:", nameE);
    lay->addRow("Роль RBAC:", roleC);
    lay->addRow("Пароль:", passE);
    auto* btns = new QHBoxLayout();
    auto* ok = new QPushButton("Создать", &dlg);
    auto* cancel = new QPushButton("Отмена", &dlg);
    btns->addStretch(); btns->addWidget(ok); btns->addWidget(cancel);
    lay->addRow(btns);
    connect(ok, &QPushButton::clicked, &dlg, &QDialog::accept);
    connect(cancel, &QPushButton::clicked, &dlg, &QDialog::reject);

    if (dlg.exec() == QDialog::Accepted) {
        QString err;
        if (!m_ctx.auth.createUser(logE->text(), nameE->text(),
                                   static_cast<UserRole>(roleC->currentData().toInt()),
                                   passE->text(), m_actor, &err)) {
            QMessageBox::warning(this, "Ошибка создания пользователя", err);
            return;
        }
        reloadUsersAndSettings();
    }
}

void DictionariesDialog::onToggleUserActive() {
    const int row = m_usersTable->currentRow();
    if (row < 0 || row >= static_cast<int>(m_users.size())) return;
    QString err;
    if (!m_ctx.auth.setUserActive(m_users[row].id, !m_users[row].active, m_actor, &err)) {
        QMessageBox::warning(this, "Ошибка", err);
        return;
    }
    reloadUsersAndSettings();
}

void DictionariesDialog::onSaveThresholds() {
    AlertThresholds th;
    th.maintHoursWarnRatio = m_maintPctSpin->value() / 100.0;
    th.maintDaysWarn = m_maintDaysSpin->value();
    th.permitDaysWarn = m_permitDaysSpin->value();
    th.batteryWarnRatio = m_batPctSpin->value() / 100.0;

    QString err;
    if (!m_ctx.fleet.saveAlertThresholds(th, m_actor, &err)) {
        QMessageBox::warning(this, "Ошибка сохранения настроек", err);
        return;
    }
    QMessageBox::information(this, "Пороги сохранены",
                             "Настройки порогов предупреждений сохранены в БД (app_settings) и применяются немедленно.");
}

} // namespace uavlog
