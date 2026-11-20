#include "ui/MainWindow.h"
#include "core/RbacGuard.h"
#include "ui/FlightDialog.h"
#include "ui/LoginDialog.h"

#include <QAction>
#include <QApplication>
#include <QBrush>
#include <QColor>
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
#include <QSortFilterProxyModel>
#include <QSplitter>
#include <QStandardItemModel>
#include <QStatusBar>
#include <QTableView>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>

namespace uavlog {

namespace {
QString formatMinutes(int totalMin) {
    return QString("%1:%2").arg(totalMin / 60).arg(totalMin % 60, 2, 10, QChar('0'));
}

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
    return anyOverdue ? "ПРОСРОЧЕНО (" + best + ")" : best;
}
} // namespace

MainWindow::MainWindow(AuthService& authService,
                       FleetService& fleetService,
                       const UserAccount& currentUser,
                       QWidget* parent)
    : QMainWindow(parent),
      m_authService(authService),
      m_fleetService(fleetService),
      m_user(currentUser) {
    setWindowTitle(QString("UAVLog 1.0 — Парк БЛА [%1 | %2]")
                       .arg(QString::fromStdString(m_user.fullName),
                            QString::fromStdString(toRussianRole(m_user.role))));
    resize(1180, 680); // Умещается в 1366x768 без горизонтальной прокрутки (REQ-INT-002)

    setupMenuAndToolbar();
    setupUi();
    refreshData();

    // Таймер автоблокировки сеанса 15 минут (REQ-SEC-003, UC-12)
    m_inactivityTimer = new QTimer(this);
    m_inactivityTimer->setInterval(15 * 60 * 1000);
    connect(m_inactivityTimer, &QTimer::timeout, this, &MainWindow::onSessionTimeout);
    m_inactivityTimer->start();
    qApp->installEventFilter(this);
}

void MainWindow::setupMenuAndToolbar() {
    auto* mFile = menuBar()->addMenu("Файл");
    mFile->addAction("Выход", this, &QWidget::close);

    auto* mDict = menuBar()->addMenu("Справочники");
    auto* aTypes = mDict->addAction("Типы БЛА и регламенты ТО");
    auto* aOps = mDict->addAction("Операторы и допуски");
    auto* aBat = mDict->addAction("Реестр АКБ");
    const bool canDict = RbacGuard::hasPermission(m_user.role, Permission::ManageDictionaries);
    aTypes->setVisible(canDict);
    aOps->setVisible(canDict);
    aBat->setVisible(canDict);

    auto* mLogs = menuBar()->addMenu("Журналы");
    auto* aFlight = mLogs->addAction("Зарегистрировать полёт", this, &MainWindow::onRegisterFlight);
    aFlight->setVisible(RbacGuard::hasPermission(m_user.role, Permission::RegisterFlight));

    auto* mReports = menuBar()->addMenu("Отчёты");
    mReports->menuAction()->setVisible(RbacGuard::hasPermission(m_user.role, Permission::GenerateReports));
    mReports->addAction("Сводные отчёты и Акт готовности парка");

    auto* mService = menuBar()->addMenu("Сервис");
    mService->menuAction()->setVisible(RbacGuard::hasPermission(m_user.role, Permission::ManageBackups));
    mService->addAction("Резервное копирование БД");
    mService->addAction("Журнал аудита (только чтение)");

    menuBar()->addMenu("Справка");

    auto* tb = addToolBar("Панель инструментов");
    tb->setMovable(false);
    if (RbacGuard::hasPermission(m_user.role, Permission::RegisterFlight)) {
        tb->addAction("Зарегистрировать полёт", this, &MainWindow::onRegisterFlight);
    }
    if (RbacGuard::hasPermission(m_user.role, Permission::RegisterPreflightCheck)) {
        tb->addAction("Предполётный осмотр (F-15)");
    }
    if (RbacGuard::hasPermission(m_user.role, Permission::RegisterMaintenance)) {
        tb->addAction("Внести ТО");
    }
    if (RbacGuard::hasPermission(m_user.role, Permission::ReportFault)) {
        tb->addAction("Неисправность");
    }
    tb->addSeparator();
    tb->addWidget(new QLabel(" Поиск (Ctrl+F): ", this));
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText("Бортовой номер, тип, статус...");
    m_searchEdit->setMaximumWidth(280);
    tb->addWidget(m_searchEdit);

    auto* scFind = new QShortcut(QKeySequence::Find, this);
    connect(scFind, &QShortcut::activated, m_searchEdit, [this]() {
        m_searchEdit->setFocus();
        m_searchEdit->selectAll();
    });
}

void MainWindow::setupUi() {
    auto* splitter = new QSplitter(Qt::Horizontal, this);

    // Левая часть: Таблица парка БЛА
    auto* leftBox = new QGroupBox("Реестр парка БЛА", splitter);
    auto* leftLayout = new QVBoxLayout(leftBox);

    m_fleetModel = new QStandardItemModel(this);
    m_fleetModel->setHorizontalHeaderLabels(
        {"Борт", "Тип БЛА", "Состояние", "Налёт, ч:мм", "Полётов", "До ТО (меньший остаток)", "Штатная нагрузка"});

    m_proxyModel = new QSortFilterProxyModel(this);
    m_proxyModel->setSourceModel(m_fleetModel);
    m_proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxyModel->setFilterKeyColumn(-1); // Фильтрация по всем столбцам (REQ-FUNC-013)

    m_fleetTable = new QTableView(leftBox);
    m_fleetTable->setModel(m_proxyModel);
    m_fleetTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_fleetTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_fleetTable->setAlternatingRowColors(true);
    m_fleetTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    leftLayout->addWidget(m_fleetTable);

    // Правая часть: Панель предупреждений (ТО, допуски, АКБ)
    auto* rightBox = new QGroupBox("Панель предупреждений (ТО, допуски, АКБ)", splitter);
    auto* rightLayout = new QVBoxLayout(rightBox);
    m_alertsList = new QListWidget(rightBox);
    m_alertsList->setWordWrap(true);
    rightLayout->addWidget(m_alertsList);

    splitter->setStretchFactor(0, 7);
    splitter->setStretchFactor(1, 3);
    setCentralWidget(splitter);

    connect(m_searchEdit, &QLineEdit::textChanged, m_proxyModel, &QSortFilterProxyModel::setFilterFixedString);
}

void MainWindow::refreshData() {
    const auto dash = m_fleetService.loadDashboard();
    m_fleetModel->removeRows(0, m_fleetModel->rowCount());

    int serviceableCount = 0;
    for (std::size_t i = 0; i < dash.uavs.size(); ++i) {
        const auto& u = dash.uavs[i];
        if (u.status == UavStatus::Serviceable) ++serviceableCount;

        QList<QStandardItem*> row;
        row << new QStandardItem(QString::fromStdString(u.tailNumber));
        row << new QStandardItem(QString::fromStdString(u.typeName));
        auto* statusItem = new QStandardItem(QString::fromStdString(toRussianStatus(u.status)));
        row << statusItem;
        row << new QStandardItem(formatMinutes(u.totalFlightMin));
        row << new QStandardItem(QString::number(u.totalFlights));
        row << new QStandardItem(formatMinRemainder(dash.remainders[i]));
        row << new QStandardItem(QString::fromStdString(u.standardPayload));

        // Цветовая + текстовая индикация (REQ-INT-003)
        if (u.status == UavStatus::Serviceable) {
            bool warn = false;
            for (const auto& r : dash.remainders[i]) if (r.isWarning) warn = true;
            statusItem->setBackground(warn ? QBrush(QColor("#FFF3CD")) : QBrush(QColor("#E6F4EA")));
            statusItem->setForeground(warn ? QBrush(QColor("#856404")) : QBrush(QColor("#137333")));
        } else if (u.status == UavStatus::DueMaintenance || u.status == UavStatus::Faulty) {
            statusItem->setBackground(QBrush(QColor("#FCE8E6")));
            statusItem->setForeground(QBrush(QColor("#A50E0E")));
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
        QString("Всего бортов: %1 | Исправны (допущены): %2 | Активных предупреждений: %3 | Роль: %4 | БД: SQLite WAL")
            .arg(dash.uavs.size())
            .arg(serviceableCount)
            .arg(dash.alerts.size())
            .arg(QString::fromStdString(toRussianRole(m_user.role))));
}

void MainWindow::onRegisterFlight() {
    FlightDialog dlg(m_fleetService, m_user, this);
    if (dlg.exec() == QDialog::Accepted) {
        refreshData();
        statusBar()->showMessage("Полёт зарегистрирован; налёт, циклы АКБ и сроки ТО пересчитаны.", 5000);
    }
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
    LoginDialog lockDlg(m_authService, this);
    lockDlg.setWindowTitle("Сеанс заблокирован (15 мин бездействия)");
    if (lockDlg.exec() != QDialog::Accepted) {
        close();
        return;
    }
    m_inactivityTimer->start();
}

} // namespace uavlog
