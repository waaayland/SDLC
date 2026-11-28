#pragma once

#include "core/DomainTypes.h"
#include "ui/AppContext.h"

#include <QDialog>
#include <vector>

class QCheckBox;
class QComboBox;
class QDateEdit;
class QLineEdit;
class QPushButton;
class QTableWidget;

namespace uavlog {

// Диалог регистрации и устранения неисправностей БЛА (REQ-FUNC-011, модуль F-10)
class FaultsDialog : public QDialog {
    Q_OBJECT
public:
    FaultsDialog(AppContext& ctx,
                 const UserAccount& actor,
                 int preselectUavId = 0,
                 QWidget* parent = nullptr);

private slots:
    void refreshTable();
    void onReportClicked();
    void onResolveClicked();

private:
    AppContext& m_ctx;
    UserAccount m_actor;
    std::vector<FaultRecord> m_faults;

    QComboBox* m_uavCombo = nullptr;
    QDateEdit* m_dateEdit = nullptr;
    QLineEdit* m_descEdit = nullptr;
    QCheckBox* m_criticalCheck = nullptr;
    QTableWidget* m_table = nullptr;
    QPushButton* m_resolveBtn = nullptr;
};

} // namespace uavlog
