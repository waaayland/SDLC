#include "core/UavStateMachine.h"

namespace uavlog {

bool UavStateMachine::canFly(UavStatus status) noexcept {
    return status == UavStatus::Serviceable;
}

bool UavStateMachine::isTerminal(UavStatus status) noexcept {
    return status == UavStatus::Retired;
}

std::optional<UavStatus> UavStateMachine::transition(UavStatus current,
                                                     UavEvent event,
                                                     const TransitionGuards& guards) noexcept {
    // Терминальное состояние: выход из «Списан» строго запрещён (REQ-FUNC-003-2)
    if (current == UavStatus::Retired) {
        return std::nullopt;
    }

    switch (event) {
        case UavEvent::Decommission:
            // Из любого рабочего состояния (Operational) -> Списан (Retired)
            return UavStatus::Retired;

        case UavEvent::MaintenanceThresholdReached:
            if (current == UavStatus::Serviceable) {
                return UavStatus::DueMaintenance;
            }
            return std::nullopt;

        case UavEvent::StartMaintenance:
            if (current == UavStatus::Serviceable || current == UavStatus::DueMaintenance) {
                return UavStatus::InMaintenance;
            }
            return std::nullopt;

        case UavEvent::CompleteMaintenance:
            if (current == UavStatus::InMaintenance ||
                current == UavStatus::DueMaintenance ||
                current == UavStatus::Serviceable) {
                if (guards.hasOpenCriticalFaults) {
                    return UavStatus::Faulty;
                }
                if (guards.hasOverdueMaintenance) {
                    return UavStatus::DueMaintenance;
                }
                return UavStatus::Serviceable;
            }
            return std::nullopt;

        case UavEvent::CriticalFaultReported:
            if (current == UavStatus::Serviceable ||
                current == UavStatus::DueMaintenance ||
                current == UavStatus::InMaintenance ||
                current == UavStatus::Faulty) {
                return UavStatus::Faulty;
            }
            return std::nullopt;

        case UavEvent::AllCriticalFaultsResolved:
            if (current == UavStatus::Faulty) {
                if (guards.hasOpenCriticalFaults) {
                    return std::nullopt;
                }
                return guards.hasOverdueMaintenance
                           ? UavStatus::DueMaintenance
                           : UavStatus::Serviceable;
            }
            return std::nullopt;
    }
    return std::nullopt;
}

} // namespace uavlog
