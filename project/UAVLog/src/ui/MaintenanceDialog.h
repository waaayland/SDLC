#pragma once

#include "core/DomainTypes.h"
#include "ui/AppContext.h"

#include <QDialog>
#include <vector>

class QComboBox;
class QDateEdit;
class QLineEdit;
class QPushButton;
class QTableWidget;

namespace uavlog {

// Диалог «Выполнение ТО» (SRS Рисунок 11, REQ-FUNC-010, модуль F-10)
class MaintenanceDialog : public QDialog {
    Q_OBJECT
public:
    MaintenanceDialog(AppContext& ctx,
                      const UserAccount& actor,
                      int preselectUavId = 0,
                      QWidget* parent = nullptr);

private slots:
    void onUavChanged();
    void onRuleChanged();
    void onStartMaintClicked();
    void onSaveClicked();

private:
    AppContext& m_ctx;
    UserAccount m_actor;
    std::vector<Uav> m_uavs;
    std::vector<MaintenanceRule> m_rulesForType;

    QComboBox* m_uavCombo = nullptr;
    QComboBox* m_ruleCombo = nullptr;
    QDateEdit* m_dateEdit = nullptr;
    QLineEdit* m_performerEdit = nullptr;
    QLineEdit* m_flightHoursEdit = nullptr;
    QTableWidget* m_worksTable = nullptr;
    QLineEdit* m_notesEdit = nullptr;
    QPushButton* m_startBtn = nullptr;
    QPushButton* m_saveBtn = nullptr;
};

} // namespace uavlog
