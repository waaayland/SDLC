#pragma once

#include "services/AuthService.h"
#include "services/FleetService.h"

#include <QMainWindow>

class QLineEdit;
class QListWidget;
class QSortFilterProxyModel;
class QStandardItemModel;
class QTableView;
class QTimer;

namespace uavlog {

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(AuthService& authService,
               FleetService& fleetService,
               const UserAccount& currentUser,
               QWidget* parent = nullptr);

    void refreshData();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void onRegisterFlight();
    void onSessionTimeout();

private:
    void setupUi();
    void setupMenuAndToolbar();

    AuthService& m_authService;
    FleetService& m_fleetService;
    UserAccount m_user;

    QLineEdit* m_searchEdit = nullptr;
    QStandardItemModel* m_fleetModel = nullptr;
    QSortFilterProxyModel* m_proxyModel = nullptr;
    QTableView* m_fleetTable = nullptr;
    QListWidget* m_alertsList = nullptr;
    QTimer* m_inactivityTimer = nullptr;
};

} // namespace uavlog
