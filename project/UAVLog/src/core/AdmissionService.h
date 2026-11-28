#pragma once

#include "core/DomainTypes.h"

namespace uavlog {

// Контракт сервиса проверки 5-факторного допуска (SDD DV-03, REQ-FUNC-006-3, REQ-FUNC-017)
class IAdmissionService {
public:
    virtual ~IAdmissionService() = default;
    virtual AdmissionResult evaluateContext(const AdmissionContext& ctx) const = 0;
};

class AdmissionService final : public IAdmissionService {
public:
    static constexpr int kMaxFlightDurationMinutes = 12 * 60; // 12 часов (720 мин)

    AdmissionResult evaluateContext(const AdmissionContext& ctx) const override;
    static AdmissionResult evaluate(const AdmissionContext& ctx);
};

} // namespace uavlog
