#pragma once

#include "core/DomainTypes.h"

namespace uavlog {

class AdmissionService {
public:
    static constexpr int kMaxFlightDurationMinutes = 12 * 60; // 12 часов (REQ-FUNC-006-3)

    static AdmissionResult evaluate(const AdmissionContext& ctx);
};

} // namespace uavlog
