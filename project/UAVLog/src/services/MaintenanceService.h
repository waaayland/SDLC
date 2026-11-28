#pragma once

#include "core/DomainTypes.h"
#include "infra/AuditLogger.h"
#include "infra/DatabaseManager.h"
#include "services/FleetService.h"

#include <QDate>
#include <QString>
#include <vector>

namespace uavlog {

struct MaintenanceWorkCheck {
    QString taskText;
    bool completed = true;
    QString note; // обязательна, если completed == false (REQ-FUNC-010)
};

// Сервис технического обслуживания и неисправностей (модуль F-10, REQ-FUNC-010, REQ-FUNC-011)
class MaintenanceService {
public:
    MaintenanceService(DatabaseManager& db, AuditLogger& audit, FleetService& fleet);

    bool startMaintenance(int uavId, const UserAccount& actor, QString* errorOut = nullptr);

    bool registerMaintenance(int uavId,
                             int ruleId,
                             const QDate& performedDate,
                             const QString& performerName,
                             const std::vector<MaintenanceWorkCheck>& works,
                             const QString& notes,
                             const UserAccount& actor,
                             int* recordIdOut = nullptr,
                             QString* errorOut = nullptr);

    std::vector<FaultRecord> loadFaults(int uavId = 0, bool onlyOpen = false);

    bool reportFault(int uavId,
                     const QDate& reportedDate,
                     const QString& description,
                     bool isCritical,
                     const UserAccount& actor,
                     int* faultIdOut = nullptr,
                     QString* errorOut = nullptr);

    bool resolveFault(int faultId,
                      const QDate& resolvedDate,
                      const QString& resolutionWorks,
                      const UserAccount& actor,
                      QString* errorOut = nullptr);

private:
    bool hasOpenCriticalFaults(int uavId);
    bool hasOverdueMaintenance(int uavId, const QDate& refDate);

    DatabaseManager& m_db;
    AuditLogger& m_audit;
    FleetService& m_fleet;
};

} // namespace uavlog
