#pragma once

#include "core/DomainTypes.h"
#include "ui/AppContext.h"

#include <QDialog>

class QLabel;
class QPushButton;
class QTabWidget;
class QTableWidget;

namespace uavlog {

// Окно «Формуляр БЛА» с 4 вкладками журналов и печатью A4 (SRS Рисунок 9, REQ-FUNC-012, F-11)
class UavFormularDialog : public QDialog {
    Q_OBJECT
public:
    UavFormularDialog(AppContext& ctx,
                      const UserAccount& actor,
                      int uavId,
                      QWidget* parent = nullptr);

    bool dataModified() const { return m_modified; }

private slots:
    void reloadFormular();
    void onEditPassportClicked();
    void onDecommissionClicked();
    void onMaintClicked();
    void onFaultClicked();
    void onExportPdfClicked();
    void onPrintClicked();

private:
    AppContext& m_ctx;
    UserAccount m_actor;
    int m_uavId = 0;
    bool m_modified = false;
    UavFormularData m_data;

    QLabel* m_passportLabel = nullptr;
    QLabel* m_statusBanner = nullptr;
    QTabWidget* m_tabs = nullptr;
    QTableWidget* m_flightsTable = nullptr;
    QTableWidget* m_maintTable = nullptr;
    QTableWidget* m_faultsTable = nullptr;
    QTableWidget* m_akbPreflightTable = nullptr;
    QTableWidget* m_preflightTable = nullptr;
    QPushButton* m_editBtn = nullptr;
    QPushButton* m_decommBtn = nullptr;
    QPushButton* m_maintBtn = nullptr;
    QPushButton* m_faultBtn = nullptr;
};

} // namespace uavlog
