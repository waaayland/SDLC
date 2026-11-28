#pragma once

#include <string>

namespace uavlog {

// Абстрактный контракт управления атомарными транзакциями (SDD DV-02, DV-03, ADR-0002)
class IUnitOfWork {
public:
    virtual ~IUnitOfWork() = default;
    virtual bool beginImmediate(std::string* errorOut = nullptr) = 0;
    virtual bool commit(std::string* errorOut = nullptr) = 0;
    virtual void rollback() noexcept = 0;
};

} // namespace uavlog
