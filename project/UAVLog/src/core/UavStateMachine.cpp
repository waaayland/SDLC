#include "core/UavStateMachine.h"

namespace uavlog {

bool UavStateMachine::canFly(UavStatus status) noexcept {
    return status == UavStatus::Serviceable;
}

std::optional<UavStatus> UavStateMachine::transition(UavStatus current,
                                                     UavEvent event,
                                                     const StateGuards& guards) noexcept {
    // Терминальное состояние: выход из «Списан» запрещён (REQ-FUNC-003-2, UML 03-state-uav-lifecycle)
    if (current == UavStatus::Retired) {
        return std::nullopt;
    }

    // Списание допускается из любого рабочего состояния (Operational -> Retired)
    if (event == UavEvent::Decommission) {
        return UavStatus::Retired;
    }

    switch (current) {
        case UavStatus::Serviceable:
            if (event == UavEvent::MaintenanceThresholdReached) return UavStatus::DueMaintenance;
            if (event == UavEvent::StartMaintenance)            return UavStatus::InMaintenance;
            if (event == UavEvent::CriticalFaultReported)       return UavStatus::Faulty;
            return std::nullopt;

        case UavStatus::DueMaintenance:
            if (event == UavEvent::StartMaintenance)            return UavStatus::InMaintenance;
            if (event == UavEvent::CriticalFaultReported)       return UavStatus::Faulty;
            return std::nullopt;

        case UavStatus::InMaintenance:
            if (event == UavEvent::CriticalFaultReported)       return UavStatus::Faulty;
            if (event == UavEvent::CompleteMaintenance) {
                if (guards.hasOpenCriticalFaults) return UavStatus::Faulty;
                if (guards.hasOverdueMaintenance) return UavStatus::DueMaintenance;
                return UavStatus::Serviceable;
            }
            return std::nullopt;

        case UavStatus::Faulty:
            if (event == UavEvent::AllCriticalFaultsResolved) {
                if (guards.hasOpenCriticalFaults) return std::nullopt;
                return guards.hasOverdueMaintenance ? UavStatus::DueMaintenance
                                                    : UavStatus::Serviceable;
            }
            return std::nullopt;

        case UavStatus::Retired:
            return std::nullopt;
    }
    return std::nullopt;
}

} // namespace uavlog
