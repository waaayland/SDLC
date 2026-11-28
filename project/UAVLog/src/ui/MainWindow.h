#pragma once

#include "core/DomainTypes.h"
#include "ui/AppContext.h"

#include <QMainWindow>
#include <QSortFilterProxyModel>

class QComboBox;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QStandardItemModel;
class QTableView;
class QTimer;

namespace uavlog {

// Прокси-фильтрация парка по тексту, типу БЛА и состоянию <= 1 с (REQ-FUNC-013, REQ-PERF-002)
class FleetFilterProxyModel : public QSortFilterProxyModel {
    Q_OBJECT
public:
    explicit FleetFilterProxyModel(QObject* parent = nullptr) : QSortFilterProxyModel(parent) {}

    void setSearchText(const QString& text) { beginFilterChange(); m_search = text.trimmed(); endFilterChange(); }
    void setTypeFilter(const QString& typeName) { beginFilterChange(); m_type = typeName; endFilterChange(); }
    void setStatusFilter(const QString& statusRu) { beginFilterChange(); m_status = statusRu; endFilterChange(); }

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    QString m_search;
    QString m_type;
    QString m_status;
};

// Главное окно «Парк БЛА» (SRS Рисунок 8, REQ-INT-002..004, REQ-SEC-002/003)
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(AppContext& ctx, const UserAccount& currentUser, QWidget* parent = nullptr);

public slots:
    void refreshData();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void onOpenFormular(const QModelIndex& proxyIdx);
    void onAlertDoubleClicked(QListWidgetItem* item);
    void onRegisterFlight();
    void onPreflightCheck();
    void onMaintenance();
    void onFaults();
    void onOpenDictionaries(int tabIndex = 0);
    void onOpenReports();
    void onOpenReadinessAct();
    void onOpenAuditLog();
    void onOpenBackupRestore();
    void onAbout();
    void onSessionTimeout();
    void onCalendarCheckTick();

private:
    void setupMenuAndToolbar();
    void setupUi();
    int selectedUavId() const;

    AppContext& m_ctx;
    UserAccount m_user;
    QDate m_lastCheckedDate;

    QStandardItemModel* m_fleetModel = nullptr;
    FleetFilterProxyModel* m_proxyModel = nullptr;
    QTableView* m_fleetTable = nullptr;
    QListWidget* m_alertsList = nullptr;
    QLineEdit* m_searchEdit = nullptr;
    QComboBox* m_typeFilterCombo = nullptr;
    QComboBox* m_statusFilterCombo = nullptr;
    QTimer* m_inactivityTimer = nullptr;
    QTimer* m_calendarTimer = nullptr;
};

} // namespace uavlog
