#include "ui/MainWindow.h"
#include "core/RbacGuard.h"
#include "ui/DictionariesDialog.h"
#include "ui/FaultsDialog.h"
#include "ui/FlightDialog.h"
#include "ui/LoginDialog.h"
#include "ui/MaintenanceDialog.h"
#include "ui/PreflightDialog.h"
#include "ui/ReportsDialog.h"
#include "ui/ServiceDialogs.h"
#include "ui/UavFormularDialog.h"

#include <QAction>
#include <QApplication>
#include <QBrush>
#include <QColor>
#include <QComboBox>
#include <QEvent>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QShortcut>
#include <QSplitter>
#include <QStandardItemModel>
#include <QStatusBar>
#include <QTableView>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>

namespace uavlog {

namespace {
QString formatMinRemainder(const std::vector<MaintenanceRemainder>& rems) {
    if (rems.empty()) return "—";
    bool anyOverdue = false;
    QString best = "В норме";
    double bestMetric = 1e9;
    for (const auto& r : rems) {
        if (r.isOverdue) anyOverdue = true;
        if (r.remainingHours.has_value() && *r.remainingHours < bestMetric) {
            bestMetric = *r.remainingHours;
            best = QString("%1 ч (%2)").arg(*r.remainingHours, 0, 'f', 1)
                       .arg(QString::fromStdString(r.ruleTitle));
        }
        if (r.remainingDays.has_value() && static_cast<double>(*r.remainingDays) < bestMetric) {
            bestMetric = static_cast<double>(*r.remainingDays);
            best = QString("%1 дн. (%2)").arg(*r.remainingDays)
                       .arg(QString::fromStdString(r.ruleTitle));
        }
    }
    return anyOverdue ? "ПРОСРОЧЕНО: " + best : best;
}
} // namespace

bool FleetFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const {
    const auto* m = sourceModel();
    if (!m) return true;

    if (!m_type.isEmpty()) {
        const QString rowType = m->index(sourceRow, 1, sourceParent).data().toString();
        if (rowType != m_type) return false;
    }
    if (!m_status.isEmpty()) {
        const QString rowStatus = m->index(sourceRow, 2, sourceParent).data().toString();
        if (rowStatus != m_status) return false;
    }
    if (!m_search.isEmpty()) {
        bool matched = false;
        for (int c = 0; c < m->columnCount(sourceParent); ++c) {
            const QString val = m->index(sourceRow, c, sourceParent).data().toString();
            if (val.contains(m_search, Qt::CaseInsensitive)) {
                matched = true;
                break;
            }
        }
        if (!matched) return false;
    }
    return true;
}

MainWindow::MainWindow(AppContext& ctx, const UserAccount& currentUser, QWidget* parent)
    : QMainWindow(parent),
      m_ctx(ctx),
      m_user(currentUser),
      m_lastCheckedDate(QDate::currentDate()) {
    setWindowTitle(QString("UAVLog 1.0 — Парк БЛА [%1 | %2]")
                       .arg(QString::fromStdString(m_user.fullName),
                            QString::fromStdString(toRussianRole(m_user.role))));
    resize(1240, 690); // Полностью умещается в 1366x768 без горизонтальной прокрутки (REQ-INT-002)

    m_ctx.fleet.recalculateCalendarDueStatuses();
    setupMenuAndToolbar();
    setupUi();
    refreshData();

    // Таймер автоблокировки сеанса через 15 минут бездействия (REQ-SEC-003, UC-12)
    m_inactivityTimer = new QTimer(this);
    m_inactivityTimer->setInterval(15 * 60 * 1000);
    connect(m_inactivityTimer, &QTimer::timeout, this, &MainWindow::onSessionTimeout);
    m_inactivityTimer->start();
    qApp->installEventFilter(this);

    // Периодическая проверка смены календарных суток (UC-12)
    m_calendarTimer = new QTimer(this);
    m_calendarTimer->setInterval(60 * 1000);
    connect(m_calendarTimer, &QTimer::timeout, this, &MainWindow::onCalendarCheckTick);
    m_calendarTimer->start();
}

void MainWindow::setupMenuAndToolbar() {
    // 6 пунктов меню в точном соответствии с мокапом SRS Рисунок 8
    auto* mFile = menuBar()->addMenu("Файл");
    mFile->addAction("Обновить данные (F5)", QKeySequence::Refresh, this, &MainWindow::refreshData);
    mFile->addAction("Заблокировать сеанс", this, &MainWindow::onSessionTimeout);
    mFile->addSeparator();
    mFile->addAction("Выход", this, &QWidget::close);

    auto* mDict = menuBar()->addMenu("Справочники");
    const bool canDict = RbacGuard::hasPermission(m_user.role, Permission::ManageDictionaries);
    mDict->menuAction()->setVisible(canDict);
    mDict->addAction("Типы БЛА и регламенты ТО", this, [this]() { onOpenDictionaries(0); });
    mDict->addAction("Шаблоны предполётных чек-листов (F-15)", this, [this]() { onOpenDictionaries(1); });
    mDict->addAction("Реестр бортов БЛА", this, [this]() { onOpenDictionaries(2); });
    mDict->addAction("Реестр АКБ", this, [this]() { onOpenDictionaries(3); });
    mDict->addAction("Операторы и допуски", this, [this]() { onOpenDictionaries(4); });
    mDict->addSeparator();
    mDict->addAction("Пользователи и пороги предупреждений", this, [this]() { onOpenDictionaries(5); });

    auto* mLogs = menuBar()->addMenu("Журналы");
    auto* aFlight = mLogs->addAction("Зарегистрировать полёт...", this, &MainWindow::onRegisterFlight);
    aFlight->setVisible(RbacGuard::hasPermission(m_user.role, Permission::RegisterFlight));
    auto* aPre = mLogs->addAction("Предполётный осмотр по чек-листу (F-15)...", this, &MainWindow::onPreflightCheck);
    aPre->setVisible(RbacGuard::hasPermission(m_user.role, Permission::RegisterPreflightCheck));
    auto* aMaint = mLogs->addAction("Зарегистрировать выполненное ТО...", this, &MainWindow::onMaintenance);
    aMaint->setVisible(RbacGuard::hasPermission(m_user.role, Permission::RegisterMaintenance));
    auto* aFault = mLogs->addAction("Журнал неисправностей...", this, &MainWindow::onFaults);
    aFault->setVisible(RbacGuard::hasPermission(m_user.role, Permission::ReportFault) ||
                       RbacGuard::hasPermission(m_user.role, Permission::ResolveFault));
    mLogs->addSeparator();
    mLogs->addAction("Открыть формуляр выбранного борта", this, [this]() {
        const auto idx = m_fleetTable->currentIndex();
        if (idx.isValid()) onOpenFormular(idx);
    });

    auto* mReports = menuBar()->addMenu("Отчёты");
    mReports->menuAction()->setVisible(RbacGuard::hasPermission(m_user.role, Permission::GenerateReports));
    mReports->addAction("Сводные отчёты (налёт, отказы, ТО)...", this, &MainWindow::onOpenReports);
    mReports->addAction("Акт готовности парка к полётам (PDF A4, F-15)...", this, &MainWindow::onOpenReadinessAct);

    auto* mService = menuBar()->addMenu("Сервис");
    const bool canBackup = RbacGuard::hasPermission(m_user.role, Permission::ManageBackups);
    const bool canAudit = RbacGuard::hasPermission(m_user.role, Permission::ViewAuditLog);
    mService->menuAction()->setVisible(canBackup || canAudit);
    auto* aBak = mService->addAction("Резервное копирование и восстановление БД...", this, &MainWindow::onOpenBackupRestore);
    aBak->setVisible(canBackup);
    auto* aAud = mService->addAction("Журнал аудита (только чтение / CSV)...", this, &MainWindow::onOpenAuditLog);
    aAud->setVisible(canAudit);

    auto* mHelp = menuBar()->addMenu("Справка");
    mHelp->addAction("О программе UAVLog 1.0", this, &MainWindow::onAbout);

    // Панель инструментов (мокап SRS Рисунок 8 + F-15)
    auto* tb = addToolBar("Панель инструментов");
    tb->setMovable(false);
    if (canDict) {
        tb->addAction("Добавить БЛА", this, [this]() { onOpenDictionaries(2); });
    }
    if (RbacGuard::hasPermission(m_user.role, Permission::RegisterFlight)) {
        tb->addAction("Зарегистрировать полёт", this, &MainWindow::onRegisterFlight);
    }
    if (RbacGuard::hasPermission(m_user.role, Permission::RegisterPreflightCheck)) {
        tb->addAction("Предполётный осмотр (F-15)", this, &MainWindow::onPreflightCheck);
    }
    if (RbacGuard::hasPermission(m_user.role, Permission::RegisterMaintenance)) {
        tb->addAction("Внести ТО", this, &MainWindow::onMaintenance);
    }
    if (RbacGuard::hasPermission(m_user.role, Permission::ReportFault) ||
        RbacGuard::hasPermission(m_user.role, Permission::ResolveFault)) {
        tb->addAction("Неисправность", this, &MainWindow::onFaults);
    }
    if (RbacGuard::hasPermission(m_user.role, Permission::GenerateReports)) {
        tb->addAction("Отчёты / Акт", this, &MainWindow::onOpenReports);
    }
    tb->addSeparator();

    tb->addWidget(new QLabel(" Поиск (Ctrl+F): ", this));
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText("Борт, тип, нагрузка...");
    m_searchEdit->setMaximumWidth(200);
    tb->addWidget(m_searchEdit);

    tb->addWidget(new QLabel(" Тип: ", this));
    m_typeFilterCombo = new QComboBox(this);
    m_typeFilterCombo->setMaximumWidth(150);
    tb->addWidget(m_typeFilterCombo);

    tb->addWidget(new QLabel(" Статус: ", this));
    m_statusFilterCombo = new QComboBox(this);
    m_statusFilterCombo->addItem("Все", "");
    m_statusFilterCombo->addItem("Исправен", "Исправен");
    m_statusFilterCombo->addItem("Требуется ТО", "Требуется ТО");
    m_statusFilterCombo->addItem("На ТО", "На ТО");
    m_statusFilterCombo->addItem("Неисправен", "Неисправен");
    m_statusFilterCombo->addItem("Списан", "Списан");
    tb->addWidget(m_statusFilterCombo);

    auto* scFind = new QShortcut(QKeySequence::Find, this);
    connect(scFind, &QShortcut::activated, m_searchEdit, [this]() {
        m_searchEdit->setFocus();
        m_searchEdit->selectAll();
    });
}

void MainWindow::setupUi() {
    auto* splitter = new QSplitter(Qt::Horizontal, this);

    // Левая панель: Таблица парка БЛА (мокап SRS Рисунок 8)
    auto* leftBox = new QGroupBox("Реестр парка БЛА (двойной щелчок — открыть электронный формуляр)", splitter);
    auto* leftLayout = new QVBoxLayout(leftBox);

    m_fleetModel = new QStandardItemModel(this);
    m_fleetModel->setHorizontalHeaderLabels(
        {"Борт", "Тип БЛА", "Состояние", "Налёт, ч:мм", "Полётов", "До ТО, ч / дн", "Последний полёт", "Штатная нагрузка"});

    m_proxyModel = new FleetFilterProxyModel(this);
    m_proxyModel->setSourceModel(m_fleetModel);

    m_fleetTable = new QTableView(leftBox);
    m_fleetTable->setModel(m_proxyModel);
    m_fleetTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_fleetTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_fleetTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_fleetTable->setAlternatingRowColors(true);
    m_fleetTable->setSortingEnabled(true);
    m_fleetTable->sortByColumn(0, Qt::AscendingOrder);
    auto* hdr = m_fleetTable->horizontalHeader();
    hdr->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    hdr->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    hdr->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    hdr->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    hdr->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    hdr->setSectionResizeMode(5, QHeaderView::Stretch);
    hdr->setSectionResizeMode(6, QHeaderView::ResizeToContents);
    hdr->setSectionResizeMode(7, QHeaderView::Stretch);
    leftLayout->addWidget(m_fleetTable);

    // Правая панель: Предупреждения (ТО, допуски, ресурс АКБ)
    auto* rightBox = new QGroupBox("Панель предупреждений (ТО, допуски, АКБ)", splitter);
    auto* rightLayout = new QVBoxLayout(rightBox);
    m_alertsList = new QListWidget(rightBox);
    m_alertsList->setWordWrap(true);
    rightLayout->addWidget(m_alertsList);

    splitter->setStretchFactor(0, 7);
    splitter->setStretchFactor(1, 3);
    setCentralWidget(splitter);

    connect(m_searchEdit, &QLineEdit::textChanged, m_proxyModel, &FleetFilterProxyModel::setSearchText);
    connect(m_typeFilterCombo, &QComboBox::currentIndexChanged, this, [this]() {
        m_proxyModel->setTypeFilter(m_typeFilterCombo->currentData().toString());
    });
    connect(m_statusFilterCombo, &QComboBox::currentIndexChanged, this, [this]() {
        m_proxyModel->setStatusFilter(m_statusFilterCombo->currentData().toString());
    });
    connect(m_fleetTable, &QTableView::doubleClicked, this, &MainWindow::onOpenFormular);
    connect(m_alertsList, &QListWidget::itemDoubleClicked, this, &MainWindow::onAlertDoubleClicked);
}

void MainWindow::refreshData() {
    // Обновляем список типов в фильтре
    const QString prevType = m_typeFilterCombo->currentData().toString();
    m_typeFilterCombo->blockSignals(true);
    m_typeFilterCombo->clear();
    m_typeFilterCombo->addItem("Все типы", "");
    for (const auto& t : m_ctx.fleet.loadUavTypes()) {
        const QString nm = QString::fromStdString(t.name);
        m_typeFilterCombo->addItem(nm, nm);
    }
    const int tIdx = m_typeFilterCombo->findData(prevType);
    if (tIdx >= 0) m_typeFilterCombo->setCurrentIndex(tIdx);
    m_typeFilterCombo->blockSignals(false);

    const auto dash = m_ctx.fleet.loadDashboard();
    m_fleetModel->removeRows(0, m_fleetModel->rowCount());

    int serviceableCount = 0;
    for (std::size_t i = 0; i < dash.uavs.size(); ++i) {
        const auto& u = dash.uavs[i];
        if (u.status == UavStatus::Serviceable) ++serviceableCount;

        auto* tailItem = new QStandardItem(QString::fromStdString(u.tailNumber));
        tailItem->setData(u.id, Qt::UserRole);
        auto* statusItem = new QStandardItem(QString::fromStdString(toRussianStatus(u.status)));

        QList<QStandardItem*> row;
        row << tailItem
            << new QStandardItem(QString::fromStdString(u.typeName))
            << statusItem
            << new QStandardItem(formatFlightMinutes(u.totalFlightMin))
            << new QStandardItem(QString::number(u.totalFlights))
            << new QStandardItem(formatMinRemainder(dash.remainders[i]))
            << new QStandardItem(formatIsoDateRu(u.lastFlightDateIso))
            << new QStandardItem(QString::fromStdString(u.standardPayload));

        // Цветовая + текстовая индикация (REQ-INT-003)
        if (u.status == UavStatus::Serviceable) {
            bool warn = false;
            for (const auto& r : dash.remainders[i]) if (r.isWarning) warn = true;
            statusItem->setBackground(warn ? QBrush(QColor("#FFF8E1")) : QBrush(QColor("#E6F4EA")));
            statusItem->setForeground(warn ? QBrush(QColor("#795500")) : QBrush(QColor("#137333")));
        } else if (u.status == UavStatus::DueMaintenance || u.status == UavStatus::Faulty) {
            statusItem->setBackground(QBrush(QColor("#FCE8E6")));
            statusItem->setForeground(QBrush(QColor("#A50E0E")));
        } else if (u.status == UavStatus::InMaintenance) {
            statusItem->setBackground(QBrush(QColor("#E8F0FE")));
            statusItem->setForeground(QBrush(QColor("#1967D2")));
        } else if (u.status == UavStatus::Retired) {
            statusItem->setBackground(QBrush(QColor("#ECEFF1")));
            statusItem->setForeground(QBrush(QColor("#455A64")));
        }
        m_fleetModel->appendRow(row);
    }

    m_alertsList->clear();
    if (dash.alerts.empty()) {
        m_alertsList->addItem("Предупреждений нет");
    } else {
        for (const auto& a : dash.alerts) {
            auto* item = new QListWidgetItem(
                (a.severity == AlertSeverity::Critical ? "[КРИТИЧНО] " : "[ВНИМАНИЕ] ") +
                QString::fromStdString(a.message));
            item->setData(Qt::UserRole, QString::fromStdString(a.entityType));
            item->setData(Qt::UserRole + 1, a.entityId);
            if (a.severity == AlertSeverity::Critical) {
                item->setBackground(QColor("#FCE8E6"));
                item->setForeground(QColor("#A50E0E"));
            } else {
                item->setBackground(QColor("#FFF8E1"));
                item->setForeground(QColor("#795500"));
            }
            m_alertsList->addItem(item);
        }
    }

    statusBar()->showMessage(
        QString("Всего бортов: %1 | Исправны: %2 | Предупреждений: %3 | Пользователь: %4 (%5) | БД: SQLite 3 WAL")
            .arg(dash.uavs.size())
            .arg(serviceableCount)
            .arg(dash.alerts.size())
            .arg(QString::fromStdString(m_user.fullName),
                 QString::fromStdString(toRussianRole(m_user.role))));
}

int MainWindow::selectedUavId() const {
    const auto proxyIdx = m_fleetTable->currentIndex();
    if (!proxyIdx.isValid()) return 0;
    const auto srcIdx = m_proxyModel->mapToSource(proxyIdx);
    return m_fleetModel->item(srcIdx.row(), 0)->data(Qt::UserRole).toInt();
}

void MainWindow::onOpenFormular(const QModelIndex& proxyIdx) {
    if (!proxyIdx.isValid()) return;
    const auto srcIdx = m_proxyModel->mapToSource(proxyIdx);
    const int uavId = m_fleetModel->item(srcIdx.row(), 0)->data(Qt::UserRole).toInt();
    if (uavId <= 0) return;

    UavFormularDialog dlg(m_ctx, m_user, uavId, this);
    dlg.exec();
    if (dlg.dataModified()) {
        refreshData();
    }
}

void MainWindow::onAlertDoubleClicked(QListWidgetItem* item) {
    if (!item) return;
    const QString entityType = item->data(Qt::UserRole).toString();
    const int entityId = item->data(Qt::UserRole + 1).toInt();
    if (entityType == "UAV" && entityId > 0) {
        // REQ-FUNC-009-2: двойной щелчок по предупреждению выделяет борт в таблице
        for (int r = 0; r < m_proxyModel->rowCount(); ++r) {
            const auto srcIdx = m_proxyModel->mapToSource(m_proxyModel->index(r, 0));
            if (m_fleetModel->item(srcIdx.row(), 0)->data(Qt::UserRole).toInt() == entityId) {
                m_fleetTable->selectRow(r);
                m_fleetTable->scrollTo(m_proxyModel->index(r, 0));
                break;
            }
        }
    } else if (entityType == "OPERATOR" && RbacGuard::hasPermission(m_user.role, Permission::ManageOperators)) {
        onOpenDictionaries(4);
    } else if (entityType == "BATTERY" && RbacGuard::hasPermission(m_user.role, Permission::ManageDictionaries)) {
        onOpenDictionaries(3);
    }
}

void MainWindow::onRegisterFlight() {
    FlightDialog dlg(m_ctx, m_user, this);
    if (selectedUavId() > 0) dlg.selectUavById(selectedUavId());
    if (dlg.exec() == QDialog::Accepted) {
        refreshData();
        statusBar()->showMessage("Полёт зарегистрирован; налёт, циклы АКБ и сроки ТО пересчитаны транзакционно.", 5000);
    }
}

void MainWindow::onPreflightCheck() {
    PreflightDialog dlg(m_ctx, m_user, selectedUavId(), QDate::currentDate(), this);
    if (dlg.exec() == QDialog::Accepted) {
        refreshData();
    }
}

void MainWindow::onMaintenance() {
    MaintenanceDialog dlg(m_ctx, m_user, selectedUavId(), this);
    if (dlg.exec() == QDialog::Accepted) {
        refreshData();
    }
}

void MainWindow::onFaults() {
    FaultsDialog dlg(m_ctx, m_user, selectedUavId(), this);
    dlg.exec();
    refreshData();
}

void MainWindow::onOpenDictionaries(int tabIndex) {
    DictionariesDialog dlg(m_ctx, m_user, tabIndex, this);
    dlg.exec();
    refreshData();
}

void MainWindow::onOpenReports() {
    ReportsDialog dlg(m_ctx, m_user, ReportKind::FlightHoursByUav, this);
    dlg.exec();
}

void MainWindow::onOpenReadinessAct() {
    ReportsDialog dlg(m_ctx, m_user, ReportKind::FleetReadinessAct, this);
    dlg.exec();
}

void MainWindow::onOpenAuditLog() {
    AuditLogDialog dlg(m_ctx, m_user, this);
    dlg.exec();
}

void MainWindow::onOpenBackupRestore() {
    BackupRestoreDialog dlg(m_ctx, m_user, this);
    dlg.exec();
    if (dlg.databaseRestored()) {
        refreshData();
    }
}

void MainWindow::onAbout() {
    QMessageBox::about(
        this, "О программе UAVLog 1.0",
        "<b>UAVLog 1.0 — Журнал эксплуатации парка БЛА</b><br/><br/>"
        "Автономное настольное ПО (C++17, Qt 6.11 Widgets, SQLite 3 WAL).<br/>"
        "Реализованы: 5-факторный допуск к полётам, предполётные чек-листы и Акт готовности (F-15), "
        "таблично-управляемый движок расчёта ТО (ADR-0004), конечный автомат БЛА (UavStateMachine), "
        "PBKDF2/RBAC/аудит (ADR-0003), отчёты CSV/PDF A4 (ADR-0005) и горячие копии VACUUM INTO (ADR-0002).<br/><br/>"
        "БГУИР, кафедра ЭВМ, гр. 434701, Осипенко О. С., 2026.");
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::MouseMove || event->type() == QEvent::KeyPress ||
        event->type() == QEvent::MouseButtonPress) {
        if (m_inactivityTimer && m_inactivityTimer->isActive()) {
            m_inactivityTimer->start();
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::onSessionTimeout() {
    m_inactivityTimer->stop();
    // Модальная блокировка поверх открытых окон без потери несохранённых данных (REQ-SEC-003)
    LoginDialog lockDlg(m_ctx.auth, QString::fromStdString(m_user.username), this);
    if (lockDlg.exec() != QDialog::Accepted) {
        close();
        return;
    }
    m_inactivityTimer->start();
}

void MainWindow::onCalendarCheckTick() {
    const QDate now = QDate::currentDate();
    if (now != m_lastCheckedDate) {
        m_lastCheckedDate = now;
        m_ctx.fleet.recalculateCalendarDueStatuses(now);
        refreshData();
    }
}

} // namespace uavlog
