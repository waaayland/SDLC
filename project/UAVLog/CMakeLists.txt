cmake_minimum_required(VERSION 3.21)
project(UAVLog VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Sql PrintSupport Test)

# 1. Domain Model Layer (uavlog_core) — чистый C++17 без Qt6::Widgets и Qt6::Sql (ADR-0001, REQ-MAINT-001)
add_library(uavlog_core STATIC
    src/core/DomainTypes.h
    src/core/Repositories.h
    src/core/UavStateMachine.h
    src/core/UavStateMachine.cpp
    src/core/AdmissionService.h
    src/core/AdmissionService.cpp
    src/core/MaintenanceEngine.h
    src/core/MaintenanceEngine.cpp
    src/core/RbacGuard.h
    src/core/RbacGuard.cpp
)
target_include_directories(uavlog_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

# 2. Infrastructure Layer (uavlog_infra) — SQLite 3 WAL, PBKDF2-HMAC-SHA256, Audit, Rotating Log, CSV/PDF A4
add_library(uavlog_infra STATIC
    src/infra/CryptoProvider.h
    src/infra/CryptoProvider.cpp
    src/infra/AuditLogger.h
    src/infra/AuditLogger.cpp
    src/infra/DatabaseManager.h
    src/infra/DatabaseManager.cpp
    src/infra/CsvExporter.h
    src/infra/CsvExporter.cpp
    src/infra/PdfExporter.h
    src/infra/PdfExporter.cpp
)
target_include_directories(uavlog_infra PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(uavlog_infra PUBLIC uavlog_core Qt6::Core Qt6::Gui Qt6::Sql Qt6::PrintSupport)

# 3. Application Service Layer (uavlog_services) — 7 прикладных сервисов (SDD DV-02 / DV-03)
add_library(uavlog_services STATIC
    src/services/AuthService.h
    src/services/AuthService.cpp
    src/services/FleetService.h
    src/services/FleetService.cpp
    src/services/FlightService.h
    src/services/FlightService.cpp
    src/services/PreflightService.h
    src/services/PreflightService.cpp
    src/services/MaintenanceService.h
    src/services/MaintenanceService.cpp
    src/services/ReportService.h
    src/services/ReportService.cpp
    src/services/BackupAuditService.h
    src/services/BackupAuditService.cpp
)
target_include_directories(uavlog_services PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(uavlog_services PUBLIC uavlog_core uavlog_infra Qt6::Core Qt6::Sql)

# 4. Presentation Layer (uavlog) — Qt 6.11 Widgets MVC GUI
add_executable(uavlog WIN32
    src/ui/AppContext.h
    src/ui/main.cpp
    src/ui/LoginDialog.h
    src/ui/LoginDialog.cpp
    src/ui/FlightDialog.h
    src/ui/FlightDialog.cpp
    src/ui/PreflightDialog.h
    src/ui/PreflightDialog.cpp
    src/ui/MaintenanceDialog.h
    src/ui/MaintenanceDialog.cpp
    src/ui/FaultsDialog.h
    src/ui/FaultsDialog.cpp
    src/ui/UavFormularDialog.h
    src/ui/UavFormularDialog.cpp
    src/ui/ReportsDialog.h
    src/ui/ReportsDialog.cpp
    src/ui/DictionariesDialog.h
    src/ui/DictionariesDialog.cpp
    src/ui/ServiceDialogs.h
    src/ui/ServiceDialogs.cpp
    src/ui/MainWindow.h
    src/ui/MainWindow.cpp
)
target_link_libraries(uavlog PRIVATE uavlog_services Qt6::Widgets Qt6::PrintSupport)

# 5. Automated Unit & Integration Tests (CTest + Qt Test)
enable_testing()
add_executable(uavlog_tests
    tests/test_uavlog_core.cpp
)
target_link_libraries(uavlog_tests PRIVATE uavlog_services Qt6::Widgets Qt6::Test)
add_test(NAME uavlog_full_suite COMMAND uavlog_tests -v1)
