#include <QtTest/QtTest>
#include <QDate>
#include <QDir>
#include <QFile>
#include <QSqlQuery>
#include <QTemporaryDir>

#include "core/AdmissionService.h"
#include "core/MaintenanceEngine.h"
#include "core/RbacGuard.h"
#include "core/UavStateMachine.h"
#include "infra/AuditLogger.h"
#include "infra/CryptoProvider.h"
#include "infra/DatabaseManager.h"
#include "services/AuthService.h"
#include "services/BackupAuditService.h"
#include "services/FleetService.h"
#include "services/FlightService.h"
#include "services/MaintenanceService.h"
#include "services/PreflightService.h"
#include "services/ReportService.h"

using namespace uavlog;

class UavLogFullTestSuite : public QObject {
    Q_OBJECT

private slots:
    // 1. Конечный автомат UavStateMachine (все допустимые и запрещённые переходы + терминальный Retired)
    void test01_StateMachineTransitions() {
        QVERIFY(UavStateMachine::canFly(UavStatus::Serviceable));
        QVERIFY(!UavStateMachine::canFly(UavStatus::DueMaintenance));
        QVERIFY(!UavStateMachine::canFly(UavStatus::InMaintenance));
        QVERIFY(!UavStateMachine::canFly(UavStatus::Faulty));
        QVERIFY(!UavStateMachine::canFly(UavStatus::Retired));
        QVERIFY(UavStateMachine::isTerminal(UavStatus::Retired));

        auto s1 = UavStateMachine::transition(UavStatus::Serviceable, UavEvent::MaintenanceThresholdReached);
        QVERIFY(s1.has_value() && *s1 == UavStatus::DueMaintenance);

        auto s2 = UavStateMachine::transition(*s1, UavEvent::StartMaintenance);
        QVERIFY(s2.has_value() && *s2 == UavStatus::InMaintenance);

        auto s3 = UavStateMachine::transition(*s2, UavEvent::CompleteMaintenance, {false, false});
        QVERIFY(s3.has_value() && *s3 == UavStatus::Serviceable);

        auto s3Due = UavStateMachine::transition(UavStatus::InMaintenance, UavEvent::CompleteMaintenance, {false, true});
        QVERIFY(s3Due.has_value() && *s3Due == UavStatus::DueMaintenance);

        auto s3Fault = UavStateMachine::transition(UavStatus::InMaintenance, UavEvent::CompleteMaintenance, {true, false});
        QVERIFY(s3Fault.has_value() && *s3Fault == UavStatus::Faulty);

        auto f1 = UavStateMachine::transition(UavStatus::Serviceable, UavEvent::CriticalFaultReported);
        QVERIFY(f1.has_value() && *f1 == UavStatus::Faulty);

        QVERIFY(!UavStateMachine::transition(*f1, UavEvent::AllCriticalFaultsResolved, {true, false}).has_value());
        auto f2 = UavStateMachine::transition(*f1, UavEvent::AllCriticalFaultsResolved, {false, false});
        QVERIFY(f2.has_value() && *f2 == UavStatus::Serviceable);
        auto f2Due = UavStateMachine::transition(*f1, UavEvent::AllCriticalFaultsResolved, {false, true});
        QVERIFY(f2Due.has_value() && *f2Due == UavStatus::DueMaintenance);

        auto ret = UavStateMachine::transition(UavStatus::Faulty, UavEvent::Decommission);
        QVERIFY(ret.has_value() && *ret == UavStatus::Retired);
        QVERIFY(!UavStateMachine::transition(UavStatus::Retired, UavEvent::Decommission).has_value());
        QVERIFY(!UavStateMachine::transition(UavStatus::Retired, UavEvent::StartMaintenance).has_value());
        QVERIFY(!UavStateMachine::transition(UavStatus::Retired, UavEvent::AllCriticalFaultsResolved).has_value());
    }

    // 2. 5-факторный контроль допуска AdmissionService (REQ-FUNC-006-3, REQ-FUNC-017)
    void test02_FiveFactorAdmissionPolicy() {
        AdmissionContext ctx;
        ctx.flightDayNumber = 2461000;
        ctx.durationMinutes = 60;

        ctx.uav.id = 1;
        ctx.uav.tailNumber = "БЛА-01";
        ctx.uav.uavTypeId = 1;
        ctx.uav.typeName = "Квадрокоптер М-30";
        ctx.uav.status = UavStatus::Serviceable;

        ctx.op.id = 1;
        ctx.op.fullName = "Соколов Д. С.";
        ctx.op.active = true;
        ctx.op.permitExpiryDayNumber = 2461050;
        ctx.op.permitExpiryIso = "2027-01-15";
        ctx.op.allowedUavTypeIds = {1};

        ctx.battery.id = 1;
        ctx.battery.inventoryNumber = "АКБ-101";
        ctx.battery.maxCycles = 200;
        ctx.battery.usedCycles = 199;

        ctx.preflight.hasInspectionOnDate = true;
        ctx.preflight.passedWithoutCriticalDefects = true;

        // Все 5 факторов выполнены
        auto ok = AdmissionService::evaluate(ctx);
        QVERIFY(ok.allowed);
        QVERIFY(ok.reasons.empty());

        // Граница длительности 720 мин (12 ч) допустима, 721 и 0 — запрещены
        auto exact12h = ctx;
        exact12h.durationMinutes = 720;
        QVERIFY(AdmissionService::evaluate(exact12h).allowed);
        exact12h.durationMinutes = 721;
        QVERIFY(!AdmissionService::evaluate(exact12h).allowed);
        exact12h.durationMinutes = 0;
        QVERIFY(!AdmissionService::evaluate(exact12h).allowed);

        // Нарушение фактора 1 (статус борта)
        auto faulty = ctx;
        faulty.uav.status = UavStatus::Faulty;
        auto r1 = AdmissionService::evaluate(faulty);
        QVERIFY(!r1.allowed && !r1.uavOk);

        // Нарушение фактора 2 (просроченное ТО)
        auto overdue = ctx;
        MaintenanceRemainder mr;
        mr.ruleTitle = "ТО-25";
        mr.isOverdue = true;
        overdue.maintRemainders = {mr};
        auto r2 = AdmissionService::evaluate(overdue);
        QVERIFY(!r2.allowed && !r2.maintOk);

        // Нарушение фактора 3 (истёк срок допуска + нет допуска на тип) -> обе причины в reasons
        auto badOp = ctx;
        badOp.op.permitExpiryDayNumber = ctx.flightDayNumber - 1;
        badOp.op.allowedUavTypeIds = {2};
        auto r3 = AdmissionService::evaluate(badOp);
        QVERIFY(!r3.allowed && !r3.operatorOk);
        QCOMPARE(r3.reasons.size(), std::size_t{2});

        // Нарушение фактора 4 (выработан ресурс АКБ 200/200)
        auto deadBat = ctx;
        deadBat.battery.usedCycles = 200;
        auto r4 = AdmissionService::evaluate(deadBat);
        QVERIFY(!r4.allowed && !r4.batteryOk);

        // Нарушение фактора 5 (F-15: нет осмотра или критичные замечания)
        auto noPf = ctx;
        noPf.preflight.hasInspectionOnDate = false;
        QVERIFY(!AdmissionService::evaluate(noPf).preflightOk);

        auto failPf = ctx;
        failPf.preflight.passedWithoutCriticalDefects = false;
        QVERIFY(!AdmissionService::evaluate(failPf).preflightOk);
    }

    // 3. Движок расчёта ТО и предупреждений «что наступит раньше» (ADR-0004, REQ-FUNC-008/009-2)
    void test03_MaintenanceEngineWhicheverFirstAndThresholds() {
        Uav uav;
        uav.id = 1;
        uav.tailNumber = "БЛА-01";
        uav.uavTypeId = 1;
        uav.totalFlightMin = 22 * 60 + 30; // 22.5 ч из 25 ч -> остаток ровно 2.5 ч == 10 % -> Warning!

        MaintenanceRule rule;
        rule.id = 1;
        rule.uavTypeId = 1;
        rule.title = "ТО-25";
        rule.intervalHours = 25.0;
        rule.intervalDays = 30;

        auto rems = MaintenanceEngine::calculateRemainders(uav, 1000, 1015, {rule}, {});
        QCOMPARE(rems.size(), std::size_t{1});
        QVERIFY(!rems[0].isOverdue);
        QVERIFY(rems[0].isWarning);

        // По налёту запас большой (5 ч), но по календарю 1030 >= 1000 + 30 -> Overdue!
        uav.totalFlightMin = 5 * 60;
        auto remsOver = MaintenanceEngine::calculateRemainders(uav, 1000, 1030, {rule}, {});
        QVERIFY(remsOver[0].isOverdue);

        // После выполнения ТО на 1030-й день при налёте 300 мин наработка обнуляется
        MaintenanceRecordSummary done{1, 5 * 60, 1030};
        auto remsAfter = MaintenanceEngine::calculateRemainders(uav, 1000, 1031, {rule}, {done});
        QVERIFY(!remsAfter[0].isOverdue);
        QVERIFY(!remsAfter[0].isWarning);

        // Порог предупреждения АКБ >= 90 % и допуск оператора <= 14 дн.
        OperatorInfo op;
        op.id = 1; op.fullName = "Петров А. И."; op.active = true;
        op.permitExpiryDayNumber = 1045; // 1045 - 1031 = 14 дн. ровно -> Warning!
        op.permitExpiryIso = "2026-12-15";

        Battery bat;
        bat.id = 1; bat.inventoryNumber = "АКБ-102"; bat.maxCycles = 100; bat.usedCycles = 90; // ровно 90 % -> Warning!

        auto alerts = MaintenanceEngine::buildAlerts({uav}, {remsAfter}, {op}, {bat}, 1031);
        QCOMPARE(alerts.size(), std::size_t{2});
    }

    // 4. Ролевая матрица RBAC (REQ-SEC-002)
    void test04_RbacMatrixEnforcement() {
        QVERIFY(RbacGuard::hasPermission(UserRole::Operator, Permission::RegisterFlight));
        QVERIFY(RbacGuard::hasPermission(UserRole::Operator, Permission::RegisterPreflightCheck));
        QVERIFY(RbacGuard::hasPermission(UserRole::Operator, Permission::ReportFault));
        QVERIFY(!RbacGuard::hasPermission(UserRole::Operator, Permission::ResolveFault));
        QVERIFY(!RbacGuard::hasPermission(UserRole::Operator, Permission::RegisterMaintenance));
        QVERIFY(!RbacGuard::hasPermission(UserRole::Operator, Permission::DecommissionUav));

        QVERIFY(RbacGuard::hasPermission(UserRole::Technician, Permission::RegisterMaintenance));
        QVERIFY(RbacGuard::hasPermission(UserRole::Technician, Permission::ResolveFault));
        QVERIFY(!RbacGuard::hasPermission(UserRole::Technician, Permission::RegisterFlight));

        QVERIFY(RbacGuard::hasPermission(UserRole::Observer, Permission::GenerateReports));
        QVERIFY(!RbacGuard::hasPermission(UserRole::Observer, Permission::ReportFault));

        QVERIFY(RbacGuard::hasPermission(UserRole::Administrator, Permission::ManageBackups));
        QVERIFY(RbacGuard::hasPermission(UserRole::Administrator, Permission::ViewAuditLog));

        bool threw = false;
        try {
            RbacGuard::requirePermission(UserRole::Operator, Permission::RegisterMaintenance);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        QVERIFY(threw);
    }

    // 5. Криптография PBKDF2-HMAC-SHA256 (тест-вектор RFC 7914 / PBKDF2-SHA256) и блокировка 5 попыток (REQ-SEC-001/005)
    void test05_CryptoPbkdf2VectorAndLockout() {
        // Эталонный тест-вектор PBKDF2-HMAC-SHA256 ("password", "salt", c=1, dkLen=32)
        const QByteArray vec1 = CryptoProvider::deriveKeyPbkdf2Sha256("password", "salt", 1, 32);
        QCOMPARE(QString::fromLatin1(vec1.toHex()),
                 QString("120fb6cffcf8b32c43e7225256c4f837a86548c92ccc35480805987cb70be17b"));

        // Эталонный тест-вектор c=2
        const QByteArray vec2 = CryptoProvider::deriveKeyPbkdf2Sha256("password", "salt", 2, 32);
        QCOMPARE(QString::fromLatin1(vec2.toHex()),
                 QString("ae4d0c95af6b46d32d0adff928f06dd02a303f8ef3c251dfd6e2d85a95474c43"));

        QVERIFY(!CryptoProvider::isPasswordPolicySatisfied("1234567"));
        QVERIFY(CryptoProvider::isPasswordPolicySatisfied("Admin1234!"));

        const QString s1 = CryptoProvider::generateSaltHex();
        const QString s2 = CryptoProvider::generateSaltHex();
        QVERIFY(s1 != s2 && s1.size() == 32);

        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        DatabaseManager db("test_auth_conn");
        QString err;
        QVERIFY2(db.openAndInitialize(tmp.filePath("auth.db"), &err), qPrintable(err));
        db.seedDemoDataIfEmpty();

        AuditLogger audit(tmp.filePath("logs"));
        AuthService auth(db, audit);

        for (int i = 0; i < 4; ++i) {
            auto bad = auth.authenticate("operator", "WrongPass!");
            QVERIFY(!bad.success && !bad.locked);
        }
        auto fifth = auth.authenticate("operator", "WrongPass!");
        QVERIFY(!fifth.success && fifth.locked);

        // 6-я попытка даже с ВЕРНЫМ паролем отклоняется (REQ-SEC-005)
        auto sixth = auth.authenticate("operator", "Oper1234!");
        QVERIFY(!sixth.success && sixth.locked);
    }

    // 6. Транзакционная регистрация, правка и удаление полёта + детерминированный пересчёт SUM/COUNT (ADR-0004)
    void test06_FlightLifecycleDeterministicRecalcAndAudit() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        DatabaseManager db("test_flight_conn");
        QString err;
        QVERIFY2(db.openAndInitialize(tmp.filePath("fleet.db"), &err), qPrintable(err));
        db.seedDemoDataIfEmpty();

        AuditLogger audit(tmp.filePath("logs"));
        AuthService auth(db, audit);
        FleetService fleet(db, audit);
        FlightService flights(db, audit, fleet);

        auto loginAdmin = auth.authenticate("admin", "Admin1234!");
        auto loginOp = auth.authenticate("operator", "Oper1234!");
        QVERIFY(loginAdmin.success && loginOp.success);

        // БЛА-01 имеет 560 мин налёта (6 полётов), АКБ-101 6 циклов, ТО-10 = 10 ч (600 мин).
        // Добавляем полёт на 50 мин -> 610 мин (> 600 мин) -> автопереход в DueMaintenance!
        FlightInputDto dto;
        dto.uavId = 1;
        dto.operatorId = 1;
        dto.batteryId = 1;
        dto.flightDate = QDate::currentDate();
        dto.takeoffTime = "10:00";
        dto.landingTime = "10:50";
        dto.durationMinutes = 50;
        dto.taskAndArea = "Интеграционный тест полёта";

        int flightId = 0;
        QVERIFY2(flights.registerFlight(dto, *loginOp.user, &flightId, &err), qPrintable(err));
        QVERIFY(flightId > 0);

        auto dash1 = fleet.loadDashboard();
        QCOMPARE(dash1.uavs[0].totalFlightMin, 610);
        QCOMPARE(dash1.uavs[0].totalFlights, 7);
        QCOMPARE(dash1.uavs[0].status, UavStatus::DueMaintenance);

        // Повторный полёт на БЛА-01 заблокирован из-за DueMaintenance
        QVERIFY(!flights.registerFlight(dto, *loginOp.user, nullptr, &err));

        // Админ исправляет длительность полёта с 50 на 20 мин (560 + 20 = 580 < 600) -> возврат в Serviceable!
        dto.landingTime = "10:20";
        dto.durationMinutes = 20;
        QVERIFY2(flights.updateFlight(flightId, dto, *loginAdmin.user, &err), qPrintable(err));
        auto dash2 = fleet.loadDashboard();
        QCOMPARE(dash2.uavs[0].totalFlightMin, 580);
        QCOMPARE(dash2.uavs[0].status, UavStatus::Serviceable);

        // Админ удаляет ошибочный полёт -> точный возврат к 560 мин и 6 циклам АКБ-101 (ADR-0004)
        QVERIFY2(flights.deleteFlight(flightId, *loginAdmin.user, &err), qPrintable(err));
        auto dash3 = fleet.loadDashboard();
        QCOMPARE(dash3.uavs[0].totalFlightMin, 560);
        QCOMPARE(dash3.uavs[0].totalFlights, 6);
    }

    // 7. Модуль F-15 (предполётные осмотры, критичное замечание -> Faulty, Акт готовности)
    void test07_PreflightChecklistsAndReadinessAct() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        DatabaseManager db("test_f15_conn");
        QString err;
        QVERIFY2(db.openAndInitialize(tmp.filePath("f15.db"), &err), qPrintable(err));
        db.seedDemoDataIfEmpty();

        AuditLogger audit(tmp.filePath("logs"));
        AuthService auth(db, audit);
        FleetService fleet(db, audit);
        PreflightService preflight(db, audit);
        FlightService flights(db, audit, fleet);

        auto loginTech = auth.authenticate("tech", "Tech1234!");
        QVERIFY(loginTech.success);

        // Шаблон для БЛА-02 (тип М-30) загружается корректно
        auto tpl = preflight.loadActiveTemplateForUav(2);
        QVERIFY(tpl.has_value() && !tpl->items.empty());

        // Проходим осмотр БЛА-02 без замечаний -> 5-й фактор допуска становится зелёным!
        std::vector<PreflightItemResult> okRes;
        for (const auto& it : tpl->items) {
            okRes.push_back({it.id, it.itemText, it.isCritical, true, ""});
        }
        bool passed = false;
        QVERIFY2(preflight.saveInspection(2, tpl->id, QDate::currentDate(), okRes, "Готов", true,
                                          *loginTech.user, &passed, nullptr, &err),
                 qPrintable(err));
        QVERIFY(passed);

        FlightInputDto dto;
        dto.uavId = 2; dto.operatorId = 1; dto.batteryId = 1;
        dto.flightDate = QDate::currentDate(); dto.durationMinutes = 30;
        QVERIFY(flights.evaluateAdmission(dto).preflightOk);

        // Фиксируем критичное замечание в осмотре БЛА-02 -> автоматический перевод в Faulty + блокировка допуска!
        okRes[0].isOk = false;
        okRes[0].comment = "Трещина лопасти";
        QVERIFY2(preflight.saveInspection(2, tpl->id, QDate::currentDate(), okRes, "Забракован", true,
                                          *loginTech.user, &passed, nullptr, &err),
                 qPrintable(err));
        QVERIFY(!passed);

        auto admAfterFail = flights.evaluateAdmission(dto);
        QVERIFY(!admAfterFail.allowed);
        QVERIFY(!admAfterFail.uavOk);
        QVERIFY(!admAfterFail.preflightOk);

        // Проверка таблицы готовности парка (БЛА-01 готов, БЛА-02 не готов)
        auto rows = preflight.buildFleetReadinessRows(QDate::currentDate());
        QVERIFY(rows.size() >= 2);
        QVERIFY(rows[0].tailNumber == "БЛА-01" && rows[0].readyForFlight);
        QVERIFY(rows[1].tailNumber == "БЛА-02" && !rows[1].readyForFlight);
    }

    // 8. Модуль F-10 (выполнение ТО с валидацией перечня работ + неисправности и RBAC устранения)
    void test08_MaintenanceAndFaultsWorkflow() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        DatabaseManager db("test_f10_conn");
        QString err;
        QVERIFY2(db.openAndInitialize(tmp.filePath("f10.db"), &err), qPrintable(err));
        db.seedDemoDataIfEmpty();

        AuditLogger audit(tmp.filePath("logs"));
        AuthService auth(db, audit);
        FleetService fleet(db, audit);
        MaintenanceService maint(db, audit, fleet);

        auto loginTech = auth.authenticate("tech", "Tech1234!");
        auto loginOp = auth.authenticate("operator", "Oper1234!");
        QVERIFY(loginTech.success && loginOp.success);

        // БЛА-03 в состоянии DueMaintenance (просрочено ТО-15, правило id=6).
        // Оператор не может вносить ТО (RBAC)
        QVERIFY(!maint.startMaintenance(3, *loginOp.user, &err));

        // Техник переводит БЛА-03 «На ТО»
        QVERIFY2(maint.startMaintenance(3, *loginTech.user, &err), qPrintable(err));

        // Невыполненная работа БЕЗ примечания должна отклоняться (REQ-FUNC-010)
        std::vector<MaintenanceWorkCheck> badWorks = {{"Осмотр консолей крыла", false, ""}};
        QVERIFY(!maint.registerMaintenance(3, 6, QDate::currentDate(), "Ковалёв И. В.", badWorks, "", *loginTech.user, nullptr, &err));

        // Регистрируем ТО-15 (id=6) и ТО-50 (id=7, если календарь) со всеми отметками -> БЛА-03 возвращается в Serviceable!
        std::vector<MaintenanceWorkCheck> okWorks = {{"Осмотр консолей крыла", true, ""}, {"Проверка тяг", true, ""}};
        QVERIFY2(maint.registerMaintenance(3, 6, QDate::currentDate(), "Ковалёв И. В.", okWorks, "Выполнено", *loginTech.user, nullptr, &err), qPrintable(err));
        QVERIFY2(maint.registerMaintenance(3, 7, QDate::currentDate(), "Ковалёв И. В.", okWorks, "Выполнено", *loginTech.user, nullptr, &err), qPrintable(err));

        auto dash = fleet.loadDashboard();
        QCOMPARE(dash.uavs[2].status, UavStatus::Serviceable);

        // БЛА-04 имеет открытую критичную неисправность (id=1).
        // Оператор не может отметить устранение (REQ-SEC-002), а техник без текста работ — тоже (REQ-FUNC-011)
        QVERIFY(!maint.resolveFault(1, QDate::currentDate(), "Заменён привод", *loginOp.user, &err));
        QVERIFY(!maint.resolveFault(1, QDate::currentDate(), "   ", *loginTech.user, &err));
        QVERIFY2(maint.resolveFault(1, QDate::currentDate(), "Заменён сервопривод элерона, проверен ход", *loginTech.user, &err), qPrintable(err));

        auto dash2 = fleet.loadDashboard();
        // После устранения единственной критичной неисправности БЛА-04 выходит из Faulty (в Serviceable или DueMaintenance по календарю)
        QVERIFY(dash2.uavs[3].status != UavStatus::Faulty);
    }

    // 9. Справочники, запрет удаления борта с историей, списание, динамические пороги (F-03/04/05a/06/09)
    void test09_DictionariesDecommissionAndThresholds() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        DatabaseManager db("test_dict_conn");
        QString err;
        QVERIFY2(db.openAndInitialize(tmp.filePath("dict.db"), &err), qPrintable(err));
        db.seedDemoDataIfEmpty();

        AuditLogger audit(tmp.filePath("logs"));
        AuthService auth(db, audit);
        FleetService fleet(db, audit);
        auto loginAdmin = auth.authenticate("admin", "Admin1234!");
        QVERIFY(loginAdmin.success);

        // Попытка удалить БЛА-01 (есть история полётов) -> ЗАПРЕЩЕНО (REQ-FUNC-003-2)
        QVERIFY(!fleet.deleteUav(1, *loginAdmin.user, &err));

        // Добавление нового борта БЕЗ истории и его удаление -> РАЗРЕШЕНО
        Uav tmpUav;
        tmpUav.tailNumber = "БЛА-99";
        tmpUav.uavTypeId = 1;
        tmpUav.serialNumber = "SN-TMP-99";
        tmpUav.commissionDateIso = QDate::currentDate().toString(Qt::ISODate).toStdString();
        tmpUav.standardPayload = "Тестовый подвес";
        int tmpId = 0;
        QVERIFY2(fleet.saveUav(tmpUav, *loginAdmin.user, &tmpId, &err), qPrintable(err));
        QVERIFY2(fleet.deleteUav(tmpId, *loginAdmin.user, &err), qPrintable(err));

        // Списание БЛА-04 -> терминальное состояние Retired, правка паспорта после списания запрещена
        QVERIFY2(fleet.decommissionUav(4, *loginAdmin.user, &err), qPrintable(err));
        auto form4 = fleet.loadFormular(4);
        QCOMPARE(form4.uav.status, UavStatus::Retired);
        QVERIFY(!fleet.saveUav(form4.uav, *loginAdmin.user, nullptr, &err));

        // Регламент ТО без обеих периодичностей отклоняется (REQ-FUNC-002)
        MaintenanceRule badRule;
        badRule.uavTypeId = 1;
        badRule.title = "Пустое ТО";
        badRule.tasks = {"Осмотр"};
        QVERIFY(!fleet.saveMaintenanceRule(badRule, *loginAdmin.user, nullptr, &err));

        // Оператор без даты окончания допуска отклоняется (REQ-FUNC-005-2)
        OperatorInfo badOp;
        badOp.fullName = "Тестов Т. Т.";
        badOp.position = "Оператор";
        badOp.permitNumber = "ДОП-00";
        badOp.permitExpiryIso = "";
        badOp.allowedUavTypeIds = {1};
        QVERIFY(!fleet.saveOperator(badOp, *loginAdmin.user, nullptr, &err));

        // Настройка порогов предупреждений в app_settings (REQ-FUNC-009-2, REQ-REUSE-001)
        AlertThresholds customTh{0.15, 10, 21, 0.85};
        QVERIFY2(fleet.saveAlertThresholds(customTh, *loginAdmin.user, &err), qPrintable(err));
        auto loadedTh = fleet.loadAlertThresholds();
        QCOMPARE(loadedTh.maintDaysWarn, 10);
        QCOMPARE(loadedTh.permitDaysWarn, 21);
        QVERIFY(qFuzzyCompare(loadedTh.maintHoursWarnRatio, 0.15));
        QVERIFY(qFuzzyCompare(loadedTh.batteryWarnRatio, 0.85));
    }

    // 10. Отчёты, экспорт CSV (UTF-8 BOM, ';') и PDF A4 + Формуляр + Бэкап/Восстановление/Ротация (F-12/13/14)
    void test10_ReportsCsvPdfBackupRestoreAndLogRotation() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        DatabaseManager db("test_rep_conn");
        QString err;
        QVERIFY2(db.openAndInitialize(tmp.filePath("rep.db"), &err), qPrintable(err));
        db.seedDemoDataIfEmpty();

        AuditLogger audit(tmp.filePath("logs"), 256); // Маленький порог 256 байт для быстрой проверки ротации
        AuthService auth(db, audit);
        FleetService fleet(db, audit);
        PreflightService preflight(db, audit);
        ReportService reports(db, fleet, preflight);
        BackupAuditService backupAudit(db, audit);

        auto loginAdmin = auth.authenticate("admin", "Admin1234!");
        auto loginOp = auth.authenticate("operator", "Oper1234!");
        QVERIFY(loginAdmin.success && loginOp.success);

        // 1) Все 5 видов отчётов + CSV UTF-8 BOM + PDF A4
        const QDate dFrom = QDate::currentDate().addDays(-90);
        const QDate dTo = QDate::currentDate();
        for (ReportKind kind : {ReportKind::FlightHoursByUav, ReportKind::FlightHoursByOperator,
                                ReportKind::FaultsSummary, ReportKind::MaintenanceExecution,
                                ReportKind::FleetReadinessAct}) {
            auto rep = reports.generateReport(kind, dFrom, dTo);
            QVERIFY(!rep.headers.isEmpty());
            QVERIFY(!rep.htmlDocument.isEmpty());
        }

        auto actRep = reports.generateReport(ReportKind::FleetReadinessAct, dFrom, dTo);
        const QString csvPath = tmp.filePath("exports/readiness.csv");
        QVERIFY2(reports.exportReportCsv(actRep, csvPath, &err), qPrintable(err));
        QFile csvF(csvPath);
        QVERIFY(csvF.open(QIODevice::ReadOnly));
        const QByteArray csvBytes = csvF.readAll();
        // Проверка UTF-8 BOM (\xEF\xBB\xBF) и разделителя ';' (REQ-INT-007)
        QVERIFY(csvBytes.startsWith(QByteArray::fromHex("EFBBBF")));
        QVERIFY(csvBytes.contains(';'));

        const QString pdfPath = tmp.filePath("exports/readiness.pdf");
        QVERIFY2(reports.exportReportPdf(actRep, pdfPath, &err), qPrintable(err));
        QFile pdfF(pdfPath);
        QVERIFY(pdfF.open(QIODevice::ReadOnly));
        QVERIFY(pdfF.read(5) == "%PDF-");

        const QString formPdf = tmp.filePath("exports/formular_1.pdf");
        QVERIFY2(reports.exportFormularPdf(1, formPdf, &err), qPrintable(err));
        QVERIFY(QFileInfo(formPdf).size() > 1000);

        // 2) Аудит доступен только Администратору (REQ-SEC-004)
        QVERIFY(backupAudit.loadAuditEntries(*loginOp.user, "", &err).empty());
        QVERIFY(!backupAudit.loadAuditEntries(*loginAdmin.user).empty());

        // 3) Ручное резервное копирование + восстановление со страховой копией (REQ-FUNC-015)
        const QString bakDir = tmp.filePath("backups");
        const QString manualBak = QDir(bakDir).filePath("manual_snapshot.db");
        QVERIFY2(backupAudit.createManualBackup(*loginAdmin.user, manualBak, &err), qPrintable(err));
        QVERIFY(DatabaseManager::verifyExternalDbIntegrity(manualBak));

        QString safetyPath;
        QVERIFY2(backupAudit.restoreFromBackup(*loginAdmin.user, manualBak, bakDir, &safetyPath, &err), qPrintable(err));
        QVERIFY(QFile::exists(safetyPath));

        // 4) Ротация автобэкапов FIFO до 30 копий (REQ-REL-002)
        const QString autoDir = tmp.filePath("auto_backups");
        QDir().mkpath(autoDir);
        for (int i = 0; i < 32; ++i) {
            QFile dummy(QDir(autoDir).filePath(QString("uavlog_auto_202611%1_120000_000.db").arg(i, 2, 10, QChar('0'))));
            QVERIFY(dummy.open(QIODevice::WriteOnly));
            dummy.write("test");
        }
        QVERIFY2(backupAudit.performAutoBackupIfNeeded(autoDir, true, nullptr, &err), qPrintable(err));
        QCOMPARE(QDir(autoDir).entryList({"uavlog_auto_*.db"}, QDir::Files).size(), BackupAuditService::kMaxAutoBackups);

        // 5) Проверка обнаружения повреждённого файла БД (REQ-REL-003)
        const QString badDbPath = tmp.filePath("corrupted.db");
        {
            QFile badF(badDbPath);
            QVERIFY(badF.open(QIODevice::WriteOnly));
            badF.write("NOT_A_VALID_SQLITE_DATABASE_CORRUPTED_BYTES");
        }
        QVERIFY(!DatabaseManager::verifyExternalDbIntegrity(badDbPath, &err));

        // 6) Проверка ротации лога ошибок (REQ-OBS-001)
        for (int i = 0; i < 15; ++i) {
            audit.logError("ERROR", "test", "Тестовое сообщение об ошибке для проверки ротации файлов лога #" + QString::number(i));
        }
        QVERIFY(QFile::exists(tmp.filePath("logs/uavlog.log")));
        QVERIFY(QFile::exists(tmp.filePath("logs/uavlog.log.1")));
    }
};

QTEST_MAIN(UavLogFullTestSuite)
#include "test_uavlog_core.moc"
