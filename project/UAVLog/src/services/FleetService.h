#pragma once

#include "core/DomainTypes.h"
#include "infra/AuditLogger.h"
#include "infra/DatabaseManager.h"

#include <QDate>
#include <QString>
#include <vector>

namespace uavlog {

struct FleetDashboardData {
    std::vector<Uav> uavs;
    std::vector<std::vector<MaintenanceRemainder>> remainders;
    std::vector<AlertItem> alerts;
    AlertThresholds thresholds;
};

struct UavFormularData {
    Uav uav;
    std::vector<MaintenanceRemainder> remainders;
    std::vector<FlightRecord> flights;
    std::vector<MaintenanceRecord> maintRecords;
    std::vector<FaultRecord> faults;
    std::vector<Battery> assignedBatteries;
    std::vector<PreflightCheckRecord> preflightChecks;
};

class FleetService {
public:
    FleetService(DatabaseManager& db, AuditLogger& audit);

    // Панель парка и формуляр борта (F-04, F-09, F-11)
    FleetDashboardData loadDashboard(const QDate& today = QDate::currentDate());
    UavFormularData loadFormular(int uavId, const QDate& today = QDate::currentDate());
    int recalculateCalendarDueStatuses(const QDate& today = QDate::currentDate());

    // Настройки порогов предупреждений (REQ-FUNC-009-2, REQ-REUSE-001)
    AlertThresholds loadAlertThresholds();
    bool saveAlertThresholds(const AlertThresholds& thresholds,
                             const UserAccount& actor,
                             QString* errorOut = nullptr);

    // Типы БЛА и регламенты ТО (F-03, REQ-FUNC-002)
    std::vector<UavType> loadUavTypes();
    bool saveUavType(const UavType& type, const UserAccount& actor, int* idOut = nullptr, QString* errorOut = nullptr);
    bool deleteUavType(int typeId, const UserAccount& actor, QString* errorOut = nullptr);

    std::vector<MaintenanceRule> loadMaintenanceRules(int filterUavTypeId = 0);
    bool saveMaintenanceRule(const MaintenanceRule& rule, const UserAccount& actor, int* idOut = nullptr, QString* errorOut = nullptr);
    bool deleteMaintenanceRule(int ruleId, const UserAccount& actor, QString* errorOut = nullptr);

    // Реестр бортов и списание (F-04, REQ-FUNC-003-2)
    std::vector<Uav> loadUavs(bool includeRetired = true);
    bool saveUav(const Uav& uav, const UserAccount& actor, int* idOut = nullptr, QString* errorOut = nullptr);
    bool deleteUav(int uavId, const UserAccount& actor, QString* errorOut = nullptr);
    bool decommissionUav(int uavId, const UserAccount& actor, QString* errorOut = nullptr);

    // Реестр АКБ (F-05a, REQ-FUNC-004-2, REQ-FUNC-016)
    std::vector<Battery> loadBatteries(bool onlyAvailableForFlight = false);
    bool saveBattery(const Battery& bat, const UserAccount& actor, int* idOut = nullptr, QString* errorOut = nullptr);
    bool retireBattery(int batteryId, const UserAccount& actor, QString* errorOut = nullptr);

    // Реестр операторов и допусков (F-06, REQ-FUNC-005-2)
    std::vector<OperatorInfo> loadOperators(bool onlyActive = false);
    bool saveOperator(const OperatorInfo& op, const UserAccount& actor, int* idOut = nullptr, QString* errorOut = nullptr);
    bool setOperatorActive(int operatorId, bool active, const UserAccount& actor, QString* errorOut = nullptr);

    // Служебные выборки для других сервисов
    std::vector<MaintenanceRecordSummary> loadLastMaintRecords(int uavId);

private:
    DatabaseManager& m_db;
    AuditLogger& m_audit;
};

} // namespace uavlog
