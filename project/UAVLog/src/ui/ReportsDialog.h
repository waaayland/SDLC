#pragma once

#include "core/DomainTypes.h"
#include "services/ReportService.h"
#include "ui/AppContext.h"

#include <QDialog>

class QComboBox;
class QDateEdit;
class QLabel;
class QTableWidget;

namespace uavlog {

// Окно «Отчёты» (SRS Рисунок 12, REQ-FUNC-014-2, REQ-FUNC-017, ADR-0005)
class ReportsDialog : public QDialog {
    Q_OBJECT
public:
    ReportsDialog(AppContext& ctx,
                  const UserAccount& actor,
                  ReportKind initialKind = ReportKind::FlightHoursByUav,
                  QWidget* parent = nullptr);

private slots:
    void onGenerateClicked();
    void onExportCsvClicked();
    void onExportPdfClicked();
    void onPrintClicked();

private:
    AppContext& m_ctx;
    UserAccount m_actor;
    ReportData m_currentReport;

    QComboBox* m_kindCombo = nullptr;
    QDateEdit* m_fromEdit = nullptr;
    QDateEdit* m_toEdit = nullptr;
    QComboBox* m_filterCombo = nullptr;
    QLabel* m_titleLabel = nullptr;
    QTableWidget* m_previewTable = nullptr;
};

} // namespace uavlog
