#pragma once

#include "core/DomainTypes.h"
#include "infra/AuditLogger.h"
#include "infra/DatabaseManager.h"

#include <QDate>
#include <QString>
#include <vector>

namespace uavlog {

struct FlightInputDto {
    int uavId = 0;
    int operatorId = 0;
    int batteryId = 0;
    QDate flightDate;
    QString takeoffTime;
    QString landingTime;
    int durationMinutes = 0;
    QString taskAndArea;
    QString notes;
};

struct FleetDashboardData {
    std::vector<Uav> uavs;
    std::vector<std::vector<MaintenanceRemainder>> remainders;
    std::vector<AlertItem> alerts;
};

class FleetService {
public:
    FleetService(DatabaseManager& db, AuditLogger& audit);

    FleetDashboardData loadDashboard(const QDate& today = QDate::currentDate());
    std::vector<OperatorInfo> loadOperators();
    std::vector<Battery> loadBatteries(bool onlyUsable = false);
    std::vector<MaintenanceRule> loadMaintenanceRules();

    AdmissionResult evaluateFlightAdmission(const FlightInputDto& dto);
    bool registerFlight(const FlightInputDto& dto, const UserAccount& actor, QString* errorOut = nullptr);

private:
    std::vector<MaintenanceRecordSummary> loadLastMaintRecords(int uavId);
    PreflightSummary loadPreflightSummary(int uavId, const QDate& date);
    int loadUavCommissionDay(int uavId);

    DatabaseManager& m_db;
    AuditLogger& m_audit;
};

} // namespace uavlog
