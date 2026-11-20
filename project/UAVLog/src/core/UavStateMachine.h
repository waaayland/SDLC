#pragma once

#include "core/DomainTypes.h"
#include <optional>

namespace uavlog {

struct StateGuards {
    bool hasOpenCriticalFaults = false;
    bool hasOverdueMaintenance = false;
};

class UavStateMachine {
public:
    static bool canFly(UavStatus status) noexcept;
    static std::optional<UavStatus> transition(UavStatus current,
                                               UavEvent event,
                                               const StateGuards& guards = {}) noexcept;
};

} // namespace uavlog
