#pragma once

#include "core/DomainTypes.h"
#include "infra/AuditLogger.h"
#include "infra/DatabaseManager.h"

#include <QDate>
#include <QString>
#include <optional>
#include <vector>

namespace uavlog {

struct FleetReadinessRow {
    int uavId = 0;
    QString tailNumber;
    QString uavType;
    QString uavStatusRu;
    QString preflightStatusRu;
    QString inspectorName;
    QString defectsSummary;
    bool readyForFlight = false;
};

// Сервис предполётных осмотров по контрольным картам (модуль F-15, REQ-FUNC-017, ADR-0006)
class PreflightService {
public:
    PreflightService(DatabaseManager& db, AuditLogger& audit);

    // Шаблоны и пункты контрольных карт (T-15.2)
    std::vector<ChecklistTemplate> loadTemplates();
    std::optional<ChecklistTemplate> loadActiveTemplateForUav(int uavId);

    bool saveTemplate(const ChecklistTemplate& tpl,
                      const UserAccount& actor,
                      int* idOut = nullptr,
                      QString* errorOut = nullptr);
    bool saveChecklistItem(const ChecklistItem& item,
                           const UserAccount& actor,
                           int* idOut = nullptr,
                           QString* errorOut = nullptr);
    bool deleteChecklistItem(int itemId, const UserAccount& actor, QString* errorOut = nullptr);

    // Регистрация результата предполётного осмотра (T-15.3)
    // При выявлении критичного замечания переводит борт в состояние Faulty через UavStateMachine
    bool saveInspection(int uavId,
                        int templateId,
                        const QDate& checkDate,
                        const std::vector<PreflightItemResult>& results,
                        const QString& notes,
                        bool createFaultOnCritical,
                        const UserAccount& actor,
                        bool* passedOut = nullptr,
                        int* checkIdOut = nullptr,
                        QString* errorOut = nullptr);

    // Данные для «Акта готовности парка БЛА к полётам» (T-15.4)
    std::vector<FleetReadinessRow> buildFleetReadinessRows(const QDate& date = QDate::currentDate());

private:
    std::vector<ChecklistItem> loadItemsForTemplate(int templateId);

    DatabaseManager& m_db;
    AuditLogger& m_audit;
};

} // namespace uavlog
