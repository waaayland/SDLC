#pragma once

#include "core/DomainTypes.h"
#include "infra/AuditLogger.h"
#include "infra/DatabaseManager.h"
#include "services/FleetService.h"

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

// Сервис регистрации полётов, 5-факторного допуска и детерминированного пересчёта (SDD DV-03/DV-05, ADR-0004)
class FlightService {
public:
    FlightService(DatabaseManager& db, AuditLogger& audit, FleetService& fleetService);

    AdmissionResult evaluateAdmission(const FlightInputDto& dto);

    bool registerFlight(const FlightInputDto& dto,
                        const UserAccount& actor,
                        int* flightIdOut = nullptr,
                        QString* errorOut = nullptr);

    bool updateFlight(int flightId,
                      const FlightInputDto& dto,
                      const UserAccount& actor,
                      QString* errorOut = nullptr);

    bool deleteFlight(int flightId,
                      const UserAccount& actor,
                      QString* errorOut = nullptr);

    std::vector<FlightRecord> loadFlights(int uavId = 0,
                                          int operatorId = 0,
                                          const QDate& fromDate = {},
                                          const QDate& toDate = {});

private:
    PreflightSummary loadPreflightSummary(int uavId, const QDate& date);
    bool recalculateUavAndBatteryTx(int uavId, int batteryId, const QDate& refDate, QString* errorOut);

    DatabaseManager& m_db;
    AuditLogger& m_audit;
    FleetService& m_fleet;
};

} // namespace uavlog
