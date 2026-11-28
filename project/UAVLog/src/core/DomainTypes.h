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
    Retired         // Списан (терминальное состояние)
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
    ManageUsersAndSettings,
    DecommissionUav,
    RegisterPreflightCheck,
    RegisterFlight,
    EditOrDeleteFlight,
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
    std::string manufacturer;
    std::string defaultPayload;
    std::string notes;
};

struct MaintenanceRule {
    int id = 0;
    int uavTypeId = 0;
    std::string title;
    std::optional<double> intervalHours; // периодичность по налёту, ч
    std::optional<int> intervalDays;     // периодичность по календарю, дней
    std::vector<std::string> tasks;      // обязательный перечень работ
};

struct MaintenanceRecordSummary {
    int ruleId = 0;
    int flightMinAtMaint = 0;
    int performedDayNumber = 0; // Юлианский номер дня для расчётов без Qt
};

struct MaintenanceRecord {
    int id = 0;
    int uavId = 0;
    std::string uavTailNumber;
    int ruleId = 0;
    std::string ruleTitle;
    std::string performedDateIso;
    std::string performer;
    int flightMinAtMaint = 0;
    std::string checklistResults;
    std::string notes;
};

struct Uav {
    int id = 0;
    std::string tailNumber;
    int uavTypeId = 0;
    std::string typeName;
    std::string serialNumber;
    std::string commissionDateIso;
    std::string standardPayload;
    std::string notes;
    UavStatus status = UavStatus::Serviceable;
    int totalFlightMin = 0;
    int totalFlights = 0;
    std::string lastFlightDateIso;
};

struct Battery {
    int id = 0;
    std::string inventoryNumber;
    std::string modelName;
    std::string commissionDateIso;
    int maxCycles = 200;
    int usedCycles = 0;
    std::optional<int> assignedUavId;
    std::string assignedUavTail;
    bool retired = false;

    double usageRatio() const noexcept {
        if (maxCycles <= 0) return 1.0;
        return static_cast<double>(usedCycles) / static_cast<double>(maxCycles);
    }

    bool isExhausted() const noexcept {
        return retired || maxCycles <= 0 || usedCycles >= maxCycles;
    }
};

struct OperatorInfo {
    int id = 0;
    std::string fullName;
    std::string position;
    std::string permitNumber;
    std::string permitExpiryIso; // YYYY-MM-DD
    int permitExpiryDayNumber = 0;
    bool active = true;
    std::vector<int> allowedUavTypeIds;
};

struct FlightRecord {
    int id = 0;
    int uavId = 0;
    std::string uavTailNumber;
    int operatorId = 0;
    std::string operatorName;
    int batteryId = 0;
    std::string batteryInventory;
    std::optional<int> preflightId;
    std::string flightDateIso;
    std::string takeoffTime;
    std::string landingTime;
    int durationMinutes = 0;
    std::string taskAndArea;
    std::string notes;
    int createdByUserId = 0;
};

struct FaultRecord {
    int id = 0;
    int uavId = 0;
    std::string uavTailNumber;
    std::string reportedDateIso;
    std::string reportedByName;
    std::string description;
    bool isCritical = false;
    std::string resolvedDateIso;
    std::string resolutionWorks;

    bool isOpen() const noexcept {
        return resolvedDateIso.empty();
    }
};

struct ChecklistItem {
    int id = 0;
    int templateId = 0;
    std::string itemText;
    bool isCritical = true;
    int sortOrder = 0;
};

struct ChecklistTemplate {
    int id = 0;
    int uavTypeId = 0;
    std::string uavTypeName;
    std::string name;
    bool active = true;
    std::string createdAtIso;
    std::vector<ChecklistItem> items;
};

struct PreflightItemResult {
    int itemId = 0;
    std::string itemText;
    bool isCritical = true;
    bool isOk = true;
    std::string comment;
};

struct PreflightCheckRecord {
    int id = 0;
    int uavId = 0;
    std::string uavTailNumber;
    int templateId = 0;
    int inspectorId = 0;
    std::string inspectorName;
    std::string checkDateIso;
    bool passed = false;
    int criticalDefectsCount = 0;
    std::string notes;
};

struct PreflightSummary {
    bool hasInspectionOnDate = false;
    bool passedWithoutCriticalDefects = false;
    std::optional<int> checkId;
    std::string inspectionDateIso;
};

struct AlertThresholds {
    double maintHoursWarnRatio = 0.10; // <= 10 % периодичности налёта (REQ-FUNC-009-2)
    int maintDaysWarn = 7;             // <= 7 суток до ТО
    int permitDaysWarn = 14;           // <= 14 суток до окончания допуска оператора
    double batteryWarnRatio = 0.90;    // >= 90 % ресурса АКБ
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
    std::string category;   // "ТО", "Допуск", "АКБ", "Неисправность"
    std::string entityType; // "UAV", "OPERATOR", "BATTERY"
    int entityId = 0;
    std::string message;
};

struct AuditEntry {
    int id = 0;
    int userId = 0;
    std::string username;
    std::string createdAtIso;
    std::string entityType;
    int entityId = 0;
    std::string action;
    std::string oldValue;
    std::string newValue;
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

inline std::string formatIsoToRuDate(const std::string& iso) {
    // Преобразование YYYY-MM-DD -> ДД.ММ.ГГГГ (REQ-INT-001) без зависимости от Qt
    if (iso.size() >= 10 && iso[4] == '-' && iso[7] == '-') {
        return iso.substr(8, 2) + "." + iso.substr(5, 2) + "." + iso.substr(0, 4);
    }
    return iso;
}

} // namespace uavlog
