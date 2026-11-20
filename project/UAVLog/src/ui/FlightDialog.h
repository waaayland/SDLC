#pragma once

#include "services/FleetService.h"
#include <QDialog>

class QComboBox;
class QDateEdit;
class QLabel;
class QLineEdit;
class QPushButton;
class QTimeEdit;

namespace uavlog {

class FlightDialog : public QDialog {
    Q_OBJECT
public:
    FlightDialog(FleetService& fleetService, const UserAccount& actor, QWidget* parent = nullptr);

    void selectUavByIndex(int idx);
    void selectOperatorByIndex(int idx);

private slots:
    void recomputeAndEvaluate();
    void onSaveClicked();

private:
    FlightInputDto buildDto() const;

    FleetService& m_fleetService;
    UserAccount m_actor;

    QComboBox* m_uavCombo = nullptr;
    QComboBox* m_opCombo = nullptr;
    QComboBox* m_batCombo = nullptr;
    QDateEdit* m_dateEdit = nullptr;
    QTimeEdit* m_takeoffEdit = nullptr;
    QTimeEdit* m_landingEdit = nullptr;
    QLineEdit* m_durationEdit = nullptr;
    QLineEdit* m_taskEdit = nullptr;
    QLabel* m_admissionBanner = nullptr;
    QPushButton* m_saveBtn = nullptr;
};

} // namespace uavlog
