#pragma once

#include <optional>
#include <string>
#include <vector>

namespace uavlog {

enum class UserRole {
    Administrator,
    Technician,
    Operator,
    Observer
};

enum class UavStatus {
    Serviceable,    // Исправен (допущен)
    DueMaintenance, // Требуется ТО
    InMaintenance,  // На ТО
    Faulty,         // Неисправен
    Retired         // Списан (терминальное)
};

enum class UavEvent {
    MaintenanceThresholdReached,
    StartMaintenance,
    CompleteMaintenance,
    CriticalFaultReported,
    AllCriticalFaultsResolved,
    Decommission
};

enum class Permission {
    ViewFleetAndFormular,
    ManageDictionaries,
    ManageOperators,
    DecommissionUav,
    RegisterPreflightCheck,
    RegisterFlight,
    RegisterMaintenance,
    ReportFault,
    ResolveFault,
    GenerateReports,
    ViewAuditLog,
    ManageBackups
};

struct UserAccount {
    int id = 0;
    std::string username;
    std::string fullName;
    UserRole role = UserRole::Observer;
    bool active = true;
};

struct UavType {
    int id = 0;
    std::string name;
    std::string description;
};

struct MaintenanceRule {
    int id = 0;
    int uavTypeId = 0;
    std::string title;
    std::optional<double> intervalHours; // периодичность по налёту (ч)
    std::optional<int> intervalDays;     // периодичность по календарю (дни)
    std::string worksChecklist;
};

struct MaintenanceRecordSummary {
    int ruleId = 0;
    int flightMinAtMaint = 0;
    int performedDayNumber = 0; // Юлианский день для детерминированных расчётов без Qt
};

struct Uav {
    int id = 0;
    std::string tailNumber;
    int uavTypeId = 0;
    std::string typeName;
    std::string serialNumber;
    std::string commissionDateIso;
    std::string standardPayload;
    UavStatus status = UavStatus::Serviceable;
    int totalFlightMin = 0;
    int totalFlights = 0;
    std::string lastFlightDateIso;
};

struct Battery {
    int id = 0;
    std::string inventoryNumber;
    std::string typeName;
    int maxCycles = 200;
    int usedCycles = 0;
    std::optional<int> assignedUavId;
    bool retired = false;
};

struct OperatorInfo {
    int id = 0;
    std::string fullName;
    std::string position;
    std::string permitNumber;
    int permitExpiryDayNumber = 0;
    std::string permitExpiryIso;
    std::vector<int> allowedUavTypeIds;
};

struct PreflightSummary {
    bool hasInspectionOnDate = false;
    bool passedWithoutCriticalDefects = false;
    std::string inspectionDateIso;
};

struct AlertThresholds {
    double maintHoursWarnRatio = 0.10; // 10 % периодичности налёта (REQ-FUNC-009-2)
    int maintDaysWarn = 7;             // 7 дней до ТО
    int permitDaysWarn = 14;           // 14 дней до окончания допуска оператора
    double batteryWarnRatio = 0.90;    // 90 % ресурса АКБ
};

struct MaintenanceRemainder {
    int ruleId = 0;
    std::string ruleTitle;
    std::optional<double> remainingHours;
    std::optional<int> remainingDays;
    bool isOverdue = false;
    bool isWarning = false;
};

struct AdmissionContext {
    Uav uav;
    OperatorInfo op;
    Battery battery;
    PreflightSummary preflight;
    std::vector<MaintenanceRemainder> maintRemainders;
    int flightDayNumber = 0;
    int durationMinutes = 0;
};

struct AdmissionResult {
    bool allowed = false;
    bool durationOk = false;
    bool uavOk = false;
    bool maintOk = false;
    bool operatorOk = false;
    bool batteryOk = false;
    bool preflightOk = false;
    std::vector<std::string> reasons;
};

enum class AlertSeverity {
    Warning,  // Жёлтый
    Critical  // Красный
};

struct AlertItem {
    AlertSeverity severity = AlertSeverity::Warning;
    std::string entityType; // "UAV", "OPERATOR", "BATTERY"
    int entityId = 0;
    std::string message;
};

inline std::string toRussianStatus(UavStatus status) {
    switch (status) {
        case UavStatus::Serviceable:    return "Исправен";
        case UavStatus::DueMaintenance: return "Требуется ТО";
        case UavStatus::InMaintenance:  return "На ТО";
        case UavStatus::Faulty:         return "Неисправен";
        case UavStatus::Retired:        return "Списан";
    }
    return "Неизвестно";
}

inline std::string toDbStatus(UavStatus status) {
    switch (status) {
        case UavStatus::Serviceable:    return "Serviceable";
        case UavStatus::DueMaintenance: return "DueMaintenance";
        case UavStatus::InMaintenance:  return "InMaintenance";
        case UavStatus::Faulty:         return "Faulty";
        case UavStatus::Retired:        return "Retired";
    }
    return "Serviceable";
}

inline UavStatus fromDbStatus(const std::string& s) {
    if (s == "DueMaintenance") return UavStatus::DueMaintenance;
    if (s == "InMaintenance")  return UavStatus::InMaintenance;
    if (s == "Faulty")         return UavStatus::Faulty;
    if (s == "Retired")        return UavStatus::Retired;
    return UavStatus::Serviceable;
}

inline std::string toRussianRole(UserRole role) {
    switch (role) {
        case UserRole::Administrator: return "Администратор";
        case UserRole::Technician:    return "Техник";
        case UserRole::Operator:      return "Оператор";
        case UserRole::Observer:      return "Наблюдатель";
    }
    return "Наблюдатель";
}

inline std::string toDbRole(UserRole role) {
    switch (role) {
        case UserRole::Administrator: return "Administrator";
        case UserRole::Technician:    return "Technician";
        case UserRole::Operator:      return "Operator";
        case UserRole::Observer:      return "Observer";
    }
    return "Observer";
}

inline UserRole fromDbRole(const std::string& s) {
    if (s == "Administrator") return UserRole::Administrator;
    if (s == "Technician")    return UserRole::Technician;
    if (s == "Operator")      return UserRole::Operator;
    return UserRole::Observer;
}

} // namespace uavlog
