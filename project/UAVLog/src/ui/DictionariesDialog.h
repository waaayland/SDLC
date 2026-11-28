#pragma once

#include "core/DomainTypes.h"
#include "ui/AppContext.h"

#include <QDialog>
#include <vector>

class QSpinBox;
class QTabWidget;
class QTableWidget;

namespace uavlog {

// Справочники и настройки системы (Администратор): F-03, T-15.2, F-04, F-05a, F-06, F-02, F-09
class DictionariesDialog : public QDialog {
    Q_OBJECT
public:
    DictionariesDialog(AppContext& ctx,
                       const UserAccount& actor,
                       int initialTab = 0,
                       QWidget* parent = nullptr);

private slots:
    // Вкладка 0: Типы БЛА и регламенты ТО (F-03)
    void reloadTypesAndRules();
    void onAddType();
    void onAddRule();
    void onDeleteRule();

    // Вкладка 1: Шаблоны предполётных чек-листов (T-15.2)
    void reloadChecklists();
    void onAddChecklistTemplate();
    void onAddChecklistItem();
    void onDeleteChecklistItem();

    // Вкладка 2: Реестр БЛА (F-04)
    void reloadUavs();
    void onAddOrEditUav(bool isEdit);
    void onDecommissionUav();
    void onDeleteUav();

    // Вкладка 3: Реестр АКБ (F-05a)
    void reloadBatteries();
    void onAddOrEditBattery(bool isEdit);
    void onRetireBattery();

    // Вкладка 4: Операторы и допуски (F-06)
    void reloadOperators();
    void onAddOrEditOperator(bool isEdit);

    // Вкладка 5: Пользователи и пороги предупреждений (F-02, F-09)
    void reloadUsersAndSettings();
    void onAddUser();
    void onToggleUserActive();
    void onSaveThresholds();

private:
    AppContext& m_ctx;
    UserAccount m_actor;

    QTabWidget* m_tabs = nullptr;

    std::vector<UavType> m_types;
    std::vector<MaintenanceRule> m_rules;
    QTableWidget* m_typesTable = nullptr;
    QTableWidget* m_rulesTable = nullptr;

    std::vector<ChecklistTemplate> m_templates;
    QTableWidget* m_tplTable = nullptr;
    QTableWidget* m_itemsTable = nullptr;

    std::vector<Uav> m_uavs;
    QTableWidget* m_uavsTable = nullptr;

    std::vector<Battery> m_batteries;
    QTableWidget* m_batsTable = nullptr;

    std::vector<OperatorInfo> m_operators;
    QTableWidget* m_opsTable = nullptr;

    std::vector<UserAccount> m_users;
    QTableWidget* m_usersTable = nullptr;
    QSpinBox* m_maintPctSpin = nullptr;
    QSpinBox* m_maintDaysSpin = nullptr;
    QSpinBox* m_permitDaysSpin = nullptr;
    QSpinBox* m_batPctSpin = nullptr;
};

} // namespace uavlog
