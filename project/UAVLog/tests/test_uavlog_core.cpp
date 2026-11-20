#include <QtTest/QtTest>
#include <QDate>
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
#include "services/FleetService.h"

using namespace uavlog;

class UavLogCoreTests : public QObject {
    Q_OBJECT

private slots:
    void testStateMachineValidAndForbiddenTransitions() {
        QVERIFY(UavStateMachine::canFly(UavStatus::Serviceable));
        QVERIFY(!UavStateMachine::canFly(UavStatus::DueMaintenance));
        QVERIFY(!UavStateMachine::canFly(UavStatus::InMaintenance));
        QVERIFY(!UavStateMachine::canFly(UavStatus::Faulty));
        QVERIFY(!UavStateMachine::canFly(UavStatus::Retired));

        // Serviceable -> DueMaintenance -> InMaintenance -> Serviceable
        auto s1 = UavStateMachine::transition(UavStatus::Serviceable, UavEvent::MaintenanceThresholdReached);
        QVERIFY(s1.has_value() && *s1 == UavStatus::DueMaintenance);

        auto s2 = UavStateMachine::transition(*s1, UavEvent::StartMaintenance);
        QVERIFY(s2.has_value() && *s2 == UavStatus::InMaintenance);

        auto s3 = UavStateMachine::transition(*s2, UavEvent::CompleteMaintenance, {false, false});
        QVERIFY(s3.has_value() && *s3 == UavStatus::Serviceable);

        // InMaintenance -> DueMaintenance если остались другие просроченные ТО
        auto s3b = UavStateMachine::transition(UavStatus::InMaintenance, UavEvent::CompleteMaintenance, {false, true});
        QVERIFY(s3b.has_value() && *s3b == UavStatus::DueMaintenance);

        // Critical fault -> Faulty -> Resolved -> Serviceable / DueMaintenance
        auto f1 = UavStateMachine::transition(UavStatus::Serviceable, UavEvent::CriticalFaultReported);
        QVERIFY(f1.has_value() && *f1 == UavStatus::Faulty);

        auto f2 = UavStateMachine::transition(*f1, UavEvent::AllCriticalFaultsResolved, {false, false});
        QVERIFY(f2.has_value() && *f2 == UavStatus::Serviceable);

        auto f2b = UavStateMachine::transition(*f1, UavEvent::AllCriticalFaultsResolved, {true, false});
        QVERIFY(!f2b.has_value()); // Нельзя вернуться, пока есть открытые критичные отказы

        // Decommission -> Retired (терминальное состояние)
        auto ret = UavStateMachine::transition(UavStatus::Faulty, UavEvent::Decommission);
        QVERIFY(ret.has_value() && *ret == UavStatus::Retired);

        // Выход из Retired строго запрещён
        QVERIFY(!UavStateMachine::transition(UavStatus::Retired, UavEvent::AllCriticalFaultsResolved).has_value());
        QVERIFY(!UavStateMachine::transition(UavStatus::Retired, UavEvent::StartMaintenance).has_value());
    }

    void testFiveFactorAdmissionService() {
        AdmissionContext ctx;
        ctx.flightDayNumber = 2461000;
        ctx.durationMinutes = 45;

        ctx.uav.id = 1;
        ctx.uav.tailNumber = "БЛА-01";
        ctx.uav.uavTypeId = 1;
        ctx.uav.typeName = "М-30";
        ctx.uav.status = UavStatus::Serviceable;

        ctx.op.id = 1;
        ctx.op.fullName = "Соколов Д. С.";
        ctx.op.permitExpiryDayNumber = 2461050;
        ctx.op.allowedUavTypeIds = {1};

        ctx.battery.id = 1;
        ctx.battery.inventoryNumber = "АКБ-101";
        ctx.battery.maxCycles = 200;
        ctx.battery.usedCycles = 199;

        ctx.preflight.hasInspectionOnDate = true;
        ctx.preflight.passedWithoutCriticalDefects = true;

        // Все 5 факторов выполнены -> допущен
        auto ok = AdmissionService::evaluate(ctx);
        QVERIFY(ok.allowed);
        QVERIFY(ok.reasons.empty());

        // Нарушение 5-го фактора (F-15): нет предполётного осмотра
        auto noPre = ctx;
        noPre.preflight.hasInspectionOnDate = false;
        auto rPre = AdmissionService::evaluate(noPre);
        QVERIFY(!rPre.allowed);
        QVERIFY(!rPre.preflightOk);

        // Нарушение 4-го фактора: АКБ выработала 100 % циклов (200/200)
        auto deadBat = ctx;
        deadBat.battery.usedCycles = 200;
        auto rBat = AdmissionService::evaluate(deadBat);
        QVERIFY(!rBat.allowed);
        QVERIFY(!rBat.batteryOk);

        // Нарушение 3-го фактора: истёк допуск и нет допуска на тип
        auto badOp = ctx;
        badOp.op.permitExpiryDayNumber = ctx.flightDayNumber - 1;
        badOp.op.allowedUavTypeIds = {2};
        auto rOp = AdmissionService::evaluate(badOp);
        QVERIFY(!rOp.allowed);
        QVERIFY(!rOp.operatorOk);
        QCOMPARE(rOp.reasons.size(), std::size_t{2});

        // Нарушение валидации длительности (> 12 ч)
        auto longFl = ctx;
        longFl.durationMinutes = 721;
        auto rDur = AdmissionService::evaluate(longFl);
        QVERIFY(!rDur.allowed);
        QVERIFY(!rDur.durationOk);
    }

    void testMaintenanceEngineThresholdsAndWhicheverFirst() {
        Uav uav;
        uav.id = 1;
        uav.tailNumber = "БЛА-01";
        uav.uavTypeId = 1;
        uav.totalFlightMin = 23 * 60; // 23.0 ч из 25 ч -> осталось 2.0 ч (8 % <= 10 % -> Warning!)

        MaintenanceRule rule;
        rule.id = 1;
        rule.uavTypeId = 1;
        rule.title = "ТО-25";
        rule.intervalHours = 25.0;
        rule.intervalDays = 30;

        // По календарю осталось 10 дней (> 7), но по налёту 2 ч <= 2.5 ч (10 %) -> предупреждение
        auto rems = MaintenanceEngine::calculateRemainders(uav, 1000, 1020, {rule}, {});
        QCOMPARE(rems.size(), std::size_t{1});
        QVERIFY(!rems[0].isOverdue);
        QVERIFY(rems[0].isWarning);

        // Просрочка по календарю (1030 >= 1000 + 30) даже при малом налёте -> Overdue!
        uav.totalFlightMin = 5 * 60;
        auto remsOver = MaintenanceEngine::calculateRemainders(uav, 1000, 1030, {rule}, {});
        QVERIFY(remsOver[0].isOverdue);

        // После выполнения ТО на 1030-й день наработка обнуляется
        MaintenanceRecordSummary rec{1, 5 * 60, 1030};
        auto remsAfter = MaintenanceEngine::calculateRemainders(uav, 1000, 1031, {rule}, {rec});
        QVERIFY(!remsAfter[0].isOverdue);
        QVERIFY(!remsAfter[0].isWarning);
    }

    void testRbacMatrix() {
        // Оператор не может вносить ТО или списывать борт (REQ-SEC-002)
        QVERIFY(RbacGuard::hasPermission(UserRole::Operator, Permission::RegisterFlight));
        QVERIFY(!RbacGuard::hasPermission(UserRole::Operator, Permission::RegisterMaintenance));
        QVERIFY(!RbacGuard::hasPermission(UserRole::Operator, Permission::DecommissionUav));

        // Техник может вносить ТО и устранять отказы, но не регистрировать полёт
        QVERIFY(RbacGuard::hasPermission(UserRole::Technician, Permission::RegisterMaintenance));
        QVERIFY(RbacGuard::hasPermission(UserRole::Technician, Permission::ResolveFault));
        QVERIFY(!RbacGuard::hasPermission(UserRole::Technician, Permission::RegisterFlight));

        // Наблюдатель — только просмотр и отчёты
        QVERIFY(RbacGuard::hasPermission(UserRole::Observer, Permission::GenerateReports));
        QVERIFY(!RbacGuard::hasPermission(UserRole::Observer, Permission::ReportFault));

        // Администратор — полный доступ
        QVERIFY(RbacGuard::hasPermission(UserRole::Administrator, Permission::ManageBackups));
        QVERIFY(RbacGuard::hasPermission(UserRole::Administrator, Permission::ViewAuditLog));
    }

    void testCryptoPbkdf2AndAuthLockout() {
        QVERIFY(!CryptoProvider::isPasswordPolicySatisfied("1234567"));
        QVERIFY(CryptoProvider::isPasswordPolicySatisfied("Admin1234!"));

        const QString salt1 = CryptoProvider::generateSaltHex();
        const QString salt2 = CryptoProvider::generateSaltHex();
        QVERIFY(salt1 != salt2);

        const QString h = CryptoProvider::hashPasswordPbkdf2("SecretPass1!", salt1, 10000);
        QVERIFY(CryptoProvider::verifyPassword("SecretPass1!", salt1, h, 10000));
        QVERIFY(!CryptoProvider::verifyPassword("WrongPass1!", salt1, h, 10000));

        // Интеграционный тест БД + блокировка после 5 неудачных попыток (REQ-SEC-005)
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        DatabaseManager db("test_auth_conn");
        QString err;
        QVERIFY2(db.openAndInitialize(tmp.filePath("test.db"), &err), qPrintable(err));
        db.seedDemoDataIfEmpty();

        AuditLogger audit(tmp.filePath("logs"));
        AuthService auth(db, audit);

        for (int i = 0; i < 4; ++i) {
            auto bad = auth.authenticate("operator", "WrongPass!");
            QVERIFY(!bad.success);
            QVERIFY(!bad.locked);
        }
        auto fifth = auth.authenticate("operator", "WrongPass!");
        QVERIFY(!fifth.success);
        QVERIFY(fifth.locked);

        // Шестая попытка с ВЕРНЫМ паролем в течение 5 минут должна отклоняться!
        auto sixth = auth.authenticate("operator", "Oper1234!");
        QVERIFY(!sixth.success);
        QVERIFY(sixth.locked);
    }

    void testTransactionalFlightRegistrationAndAudit() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        DatabaseManager db("test_flight_conn");
        QString err;
        QVERIFY2(db.openAndInitialize(tmp.filePath("fleet.db"), &err), qPrintable(err));
        db.seedDemoDataIfEmpty();

        AuditLogger audit(tmp.filePath("logs"));
        AuthService auth(db, audit);
        FleetService fleet(db, audit);

        auto loginOp = auth.authenticate("operator", "Oper1234!");
        QVERIFY(loginOp.success && loginOp.user.has_value());

        // Полёт на БЛА-01 (исправен, ТО в норме, осмотр пройден, АКБ-101 45/200) -> успех
        // Налёт БЛА-01 был 1410 мин (23.5 ч); +100 мин -> 1510 мин (> 25 ч = 1500 мин -> переход в DueMaintenance!)
        FlightInputDto dto;
        dto.uavId = 1;
        dto.operatorId = 1;
        dto.batteryId = 1;
        dto.flightDate = QDate::currentDate();
        dto.takeoffTime = "10:00";
        dto.landingTime = "11:40";
        dto.durationMinutes = 100;
        dto.taskAndArea = "Тестовый облёт, полигон";

        QVERIFY2(fleet.registerFlight(dto, *loginOp.user, &err), qPrintable(err));

        // Проверяем пересчёт налёта, циклов, смену состояния БЛА-01 -> DueMaintenance и запись в audit_log
        auto dash = fleet.loadDashboard();
        QCOMPARE(dash.uavs[0].totalFlightMin, 1510);
        QCOMPARE(dash.uavs[0].status, UavStatus::DueMaintenance);

        {
            QSqlQuery qa(db.database());
            QVERIFY(qa.exec("SELECT COUNT(*) FROM audit_log WHERE action = 'FLIGHT_CREATE';") && qa.next());
            QCOMPARE(qa.value(0).toInt(), 1);
            qa.finish();
        }

        // Повторный полёт на том же БЛА-01 теперь ЗАБЛОКИРОВАН (статус DueMaintenance + просрочено ТО-25)
        QVERIFY(!fleet.registerFlight(dto, *loginOp.user, &err));

        // Проверка резервного копирования VACUUM INTO (REQ-FUNC-015)
        QVERIFY2(db.createBackupVacuumInto(tmp.filePath("backup.db"), &err), qPrintable(err));
        QVERIFY(QFile::exists(tmp.filePath("backup.db")));
    }
};

QTEST_MAIN(UavLogCoreTests)
#include "test_uavlog_core.moc"
