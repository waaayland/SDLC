#pragma once

#include "core/DomainTypes.h"
#include "ui/AppContext.h"

#include <QDialog>

class QComboBox;
class QDateEdit;
class QLabel;
class QLineEdit;
class QPushButton;
class QTimeEdit;

namespace uavlog {

// Диалог «Регистрация полёта» с 5-факторным контролем допуска (SRS Рисунок 10, REQ-FUNC-006-3, REQ-INT-004)
class FlightDialog : public QDialog {
    Q_OBJECT
public:
    FlightDialog(AppContext& ctx, const UserAccount& actor, QWidget* parent = nullptr);

    void selectUavById(int uavId);
    void selectUavByIndex(int idx);
    void selectOperatorByIndex(int idx);

private slots:
    void recomputeAndEvaluate();
    void onSaveClicked();
    void onQuickPreflightClicked();

private:
    FlightInputDto buildDto() const;

    AppContext& m_ctx;
    UserAccount m_actor;

    QComboBox* m_uavCombo = nullptr;
    QComboBox* m_opCombo = nullptr;
    QComboBox* m_batCombo = nullptr;
    QDateEdit* m_dateEdit = nullptr;
    QTimeEdit* m_takeoffEdit = nullptr;
    QTimeEdit* m_landingEdit = nullptr;
    QLineEdit* m_durationEdit = nullptr;
    QLineEdit* m_taskEdit = nullptr;
    QLineEdit* m_notesEdit = nullptr;
    QLabel* m_admissionBanner = nullptr;
    QPushButton* m_preflightQuickBtn = nullptr;
    QPushButton* m_saveBtn = nullptr;
};

} // namespace uavlog
