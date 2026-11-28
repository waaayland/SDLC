#pragma once

#include "core/DomainTypes.h"
#include "ui/AppContext.h"

#include <QDate>
#include <QDialog>

class QCheckBox;
class QComboBox;
class QDateEdit;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;

namespace uavlog {

// Диалог предполётного осмотра по контрольной карте (модуль F-15, T-15.3, REQ-FUNC-017)
class PreflightDialog : public QDialog {
    Q_OBJECT
public:
    PreflightDialog(AppContext& ctx,
                    const UserAccount& actor,
                    int preselectUavId = 0,
                    const QDate& date = QDate::currentDate(),
                    QWidget* parent = nullptr);

private slots:
    void loadChecklistForSelectedUav();
    void updateVerdictBanner();
    void onSaveClicked();

private:
    AppContext& m_ctx;
    UserAccount m_actor;
    int m_currentTemplateId = 0;
    std::vector<ChecklistItem> m_currentItems;

    QComboBox* m_uavCombo = nullptr;
    QDateEdit* m_dateEdit = nullptr;
    QLabel* m_templateLabel = nullptr;
    QTableWidget* m_table = nullptr;
    QLineEdit* m_notesEdit = nullptr;
    QCheckBox* m_autoFaultCheck = nullptr;
    QLabel* m_verdictBanner = nullptr;
    QPushButton* m_saveBtn = nullptr;
};

} // namespace uavlog
