#pragma once

#include "core/DomainTypes.h"
#include "infra/DatabaseManager.h"
#include "services/FleetService.h"
#include "services/PreflightService.h"

#include <QDate>
#include <QString>
#include <QStringList>
#include <QWidget>
#include <vector>

namespace uavlog {

enum class ReportKind {
    FlightHoursByUav,
    FlightHoursByOperator,
    FaultsSummary,
    MaintenanceExecution,
    FleetReadinessAct // Модуль F-15 (T-15.4, REQ-FUNC-017)
};

struct ReportData {
    ReportKind kind = ReportKind::FlightHoursByUav;
    QString title;
    QString periodText;
    QString generatedAtText;
    QStringList headers;
    std::vector<QStringList> rows;
    QStringList totalsRow;
    QString htmlDocument;
};

// Сервис сводной отчётности, Акта готовности парка и печати формуляра (F-12, T-15.4, ADR-0005)
class ReportService {
public:
    ReportService(DatabaseManager& db, FleetService& fleet, PreflightService& preflight);

    ReportData generateReport(ReportKind kind,
                              const QDate& fromDate,
                              const QDate& toDate,
                              int uavFilterId = 0,
                              int operatorFilterId = 0,
                              const QString& signerName = "Начальник службы эксплуатации БЛА");

    QString buildFormularHtml(int uavId, const QDate& today = QDate::currentDate());

    bool exportReportCsv(const ReportData& report, const QString& filePath, QString* errorOut = nullptr);
    bool exportReportPdf(const ReportData& report, const QString& filePath, QString* errorOut = nullptr);
    bool printReportA4(const ReportData& report, QWidget* parent = nullptr, QString* errorOut = nullptr);

    bool exportFormularPdf(int uavId, const QString& filePath, QString* errorOut = nullptr);
    bool printFormularA4(int uavId, QWidget* parent = nullptr, QString* errorOut = nullptr);

private:
    static QString formatMinutes(int totalMin);
    static QString formatDateRu(const QString& isoDate);
    static QString buildHtmlTableDocument(const QString& title,
                                          const QString& periodText,
                                          const QString& generatedAtText,
                                          const QStringList& headers,
                                          const std::vector<QStringList>& rows,
                                          const QStringList& totalsRow,
                                          const QString& signerLine);

    DatabaseManager& m_db;
    FleetService& m_fleet;
    PreflightService& m_preflight;
};

} // namespace uavlog
