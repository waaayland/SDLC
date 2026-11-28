#pragma once

#include "core/DomainTypes.h"
#include <optional>

namespace uavlog {

struct TransitionGuards {
    bool hasOpenCriticalFaults = false;
    bool hasOverdueMaintenance = false;
};

class UavStateMachine {
public:
    static bool canFly(UavStatus status) noexcept;
    static bool isTerminal(UavStatus status) noexcept;

    // Возвращает целевое состояние при допустимом переходе или std::nullopt при запрещённом
    static std::optional<UavStatus> transition(UavStatus current,
                                               UavEvent event,
                                               const TransitionGuards& guards = {}) noexcept;
};

} // namespace uavlog
