#pragma once

#include "infra/AuditLogger.h"
#include "infra/DatabaseManager.h"
#include "services/AuthService.h"
#include "services/BackupAuditService.h"
#include "services/FleetService.h"
#include "services/FlightService.h"
#include "services/MaintenanceService.h"
#include "services/PreflightService.h"
#include "services/ReportService.h"

#include <QDate>
#include <QString>

namespace uavlog {

struct AppContext {
    DatabaseManager& db;
    AuditLogger& audit;
    AuthService& auth;
    FleetService& fleet;
    FlightService& flights;
    PreflightService& preflight;
    MaintenanceService& maint;
    ReportService& reports;
    BackupAuditService& backupAudit;
    QString backupsDir;
    QString exportsDir;
};

inline QString formatFlightMinutes(int totalMin) {
    return QString("%1:%2").arg(totalMin / 60).arg(totalMin % 60, 2, 10, QChar('0'));
}

inline QString formatIsoDateRu(const std::string& iso) {
    const QDate d = QDate::fromString(QString::fromStdString(iso), Qt::ISODate);
    return d.isValid() ? d.toString("dd.MM.yyyy") : (iso.empty() ? "—" : QString::fromStdString(iso));
}

} // namespace uavlog
