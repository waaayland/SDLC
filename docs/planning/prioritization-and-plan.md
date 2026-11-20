# Приоритизация функционала и планирование итераций — UAVLog 1.0

**Версия документа:** 1.0 (первичная оценка Архитектора до Раунда 1)  
**Дата:** 2026-11-16  
**Автор:** Осипенко О. С. (Архитектор системы / Ведущий разработчик)  
**Заказчик:** Мороз А. А. (Начальник службы эксплуатации БЛА)

---

## 1. Правила приоритизации и расчёта стоимости

- **Шкала приоритетов (Priority Scale):** ранжирование от **10** (наивысший, критически важный для запуска и безопасности полётов) до **50** (минимальный, вспомогательный или откладываемый функционал). Наименования функциональных модулей и обоснования приоритетов приведены на английском языке.
- **Длина итерации:** 1 рабочая неделя (5 рабочих дней × 8 ч = **40 чел.-ч** на одного разработчика).
- **Старт разработки:** 23.11.2026. **Фиксированный дедлайн (CON-6):** 18.12.2026 (4 итерации = 160 ч на 1 разработчика).
- **Ставка Ведущего разработчика (Architect / Middle C++ Qt):** **75 BYN/ч** (600 BYN/день, 3 000 BYN/неделя).

---

## 2. План v1 — Первичная приоритизация и оценка сроков (до обсуждения с Заказчиком)

| ID | Feature Name (English) | SRS Traceability | Dependencies | Initial Priority (10–50) | Effort (hours) | Duration (days) | Cost (BYN) |
|----|------------------------|------------------|--------------|--------------------------|----------------|-----------------|------------|
| F-01 | Core SQLite WAL Database Schema & App Bootstrap | REQ-INT-006, REQ-REL-001/003, REQ-INST-001, REQ-BUILD-001 | — | 10 (Critical) | 12 | 1.5 | 900 |
| F-02 | User Authentication (PBKDF2), RBAC & Session Lock | REQ-FUNC-001, REQ-SEC-001..003, REQ-SEC-005 | F-01 | 10 (Critical) | 14 | 1.75 | 1 050 |
| F-03 | UAV Types & Data-Driven Maintenance Regulations | REQ-FUNC-002, REQ-REUSE-001 | F-01, F-02 | 15 (High) | 10 | 1.25 | 750 |
| F-04 | UAV Fleet Registry, Lifecycle States & Decommissioning | REQ-FUNC-003-2, REQ-INT-001..003 | F-03 | 10 (Critical) | 12 | 1.5 | 900 |
| F-05 | Battery (AKB) Cycle Tracking & Payload Assignment History | REQ-FUNC-004, REQ-FUNC-016 | F-04 | 25 (Medium) | 16 | 2.0 | 1 200 |
| F-06 | Operators Registry & Qualification Expiry Control | REQ-FUNC-005-2 | F-03 | 15 (High) | 12 | 1.5 | 900 |
| F-07 | Flight Registration & Automated Admission Control | REQ-FUNC-006-2, REQ-INT-004, REQ-MAINT-001 | F-04, F-05, F-06 | 10 (Critical) | 16 | 2.0 | 1 200 |
| F-08 | Flight Hours, AKB Cycles & Maintenance Countdown Engine | REQ-FUNC-007, REQ-FUNC-008, REQ-PERF-001 | F-03, F-07 | 10 (Critical) | 14 | 1.75 | 1 050 |
| F-09 | Proactive Alerts Dashboard (TO, Permits, AKB Thresholds) | REQ-FUNC-009-2, REQ-INT-003 | F-08 | 20 (High) | 10 | 1.25 | 750 |
| F-10 | Maintenance Execution & Faults/Defects Management | REQ-FUNC-010, REQ-FUNC-011 | F-04, F-08 | 15 (High) | 12 | 1.5 | 900 |
| F-11 | UAV Electronic Logbook (Formular) & Live Search/Filter | REQ-FUNC-012, REQ-FUNC-013, REQ-PERF-002 | F-07, F-10 | 20 (High) | 12 | 1.5 | 900 |
| F-12 | Reporting Suite, CSV/PDF Export & A4 Printing | REQ-FUNC-014-2, REQ-INT-005/007, REQ-COMP-001, REQ-PERF-003 | F-08, F-10 | 25 (Medium) | 20 | 2.5 | 1 500 |
| F-13 | Immutable Audit Trail (with Visual Diff GUI) & Error Log Rotation | REQ-SEC-004, REQ-OBS-001 | F-02 | 35 (Low) | 12 | 1.5 | 900 |
| F-14 | Backup/Restore, Portable ZIP Packaging & Acceptance QA | REQ-FUNC-015, REQ-REL-002, REQ-DIST-001, REQ-COMP-002, REQ-PERF-004 | F-01..F-13 | 30 (Medium-Low) | 8 | 1.0 | 600 |
| **ИТОГО** | **14 функциональных модулей (План v1)** | **49 требований SRS** | — | **10–35** | **180 ч** | **22.5 дн. (4.5 нед.)** | **13 500 BYN** |

**Вывод по Плану v1:** при работе 1 разработчика объём **180 ч** требует **4.5 рабочих недели (5 итераций)**, что даёт ориентировочную дату релиза **23.12.2026** (превышение целевого срока 18.12.2026 на 3 рабочих дня) и стоимость **13 500 BYN**. План направлен Заказчику на обсуждение (Раунд 1).
