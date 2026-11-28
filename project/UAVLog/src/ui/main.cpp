#include "infra/AuditLogger.h"
#include "infra/DatabaseManager.h"
#include "services/AuthService.h"
#include "services/BackupAuditService.h"
#include "services/FleetService.h"
#include "services/FlightService.h"
#include "services/MaintenanceService.h"
#include "services/PreflightService.h"
#include "services/ReportService.h"
#include "ui/AppContext.h"
#include "ui/FlightDialog.h"
#include "ui/LoginDialog.h"
#include "ui/MainWindow.h"
#include "ui/PreflightDialog.h"
#include "ui/ReportsDialog.h"
#include "ui/UavFormularDialog.h"

#include <QApplication>
#include <QDir>
#include <QFileDialog>
#include <QMessageBox>
#include <QPixmap>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("UAVLog");
    app.setApplicationVersion("1.0.0");
    app.setStyle("Fusion");

    // Единая светлая деловая палитра (хорошая читаемость при дневном свете в полевых условиях)
    QPalette lightPal;
    lightPal.setColor(QPalette::Window, QColor(248, 249, 250));
    lightPal.setColor(QPalette::WindowText, Qt::black);
    lightPal.setColor(QPalette::Base, Qt::white);
    lightPal.setColor(QPalette::AlternateBase, QColor(242, 245, 248));
    lightPal.setColor(QPalette::Text, Qt::black);
    lightPal.setColor(QPalette::Button, QColor(238, 241, 244));
    lightPal.setColor(QPalette::ButtonText, Qt::black);
    lightPal.setColor(QPalette::Highlight, QColor(43, 91, 132));
    lightPal.setColor(QPalette::HighlightedText, Qt::white);
    app.setPalette(lightPal);

    const QString baseDir    = QCoreApplication::applicationDirPath();
    const QString dataDir    = QDir(baseDir).filePath("data");
    const QString logDir     = QDir(baseDir).filePath("logs");
    const QString backupsDir = QDir(baseDir).filePath("backups");
    const QString exportsDir = QDir(baseDir).filePath("exports");
    const QString dbPath     = QDir(dataDir).filePath("uavlog.db");

    uavlog::AuditLogger audit(logDir);
    uavlog::DatabaseManager db;
    QString err;

    // REQ-REL-003: проверка целостности при старте с предложением восстановления из последней резервной копии
    if (!db.openAndInitialize(dbPath, &err)) {
        audit.logError("ERROR", "main", "Ошибка открытия/целостности БД: " + err);
        const auto backups = uavlog::BackupAuditService::listBackupFiles(backupsDir);
        const QString hint = backups.isEmpty()
            ? "В каталоге backups/ резервные копии не найдены. Выберите файл копии вручную?"
            : ("Найдена последняя резервная копия:\n" + backups.first() + "\n\nВосстановить БД из копии?");

        if (QMessageBox::critical(nullptr, "Повреждение базы данных UAVLog (REQ-REL-003)",
                                  "Не удалось открыть или проверить целостность БД:\n" + err + "\n\n" + hint,
                                  QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
            const QString chosen = backups.isEmpty()
                ? QFileDialog::getOpenFileName(nullptr, "Выберите резервную копию БД", backupsDir, "SQLite DB (*.db)")
                : backups.first();
            if (chosen.isEmpty() || !db.restoreFromBackup(chosen, backupsDir, nullptr, &err)) {
                QMessageBox::critical(nullptr, "Восстановление не удалось", err);
                return 1;
            }
        } else {
            return 1;
        }
    }
    db.seedDemoDataIfEmpty();

    // Инициализация всех 7 прикладных сервисов (SDD DV-02 / DV-03)
    uavlog::AuthService authService(db, audit);
    uavlog::FleetService fleetService(db, audit);
    uavlog::FlightService flightService(db, audit, fleetService);
    uavlog::PreflightService preflightService(db, audit);
    uavlog::MaintenanceService maintService(db, audit, fleetService);
    uavlog::ReportService reportService(db, fleetService, preflightService);
    uavlog::BackupAuditService backupAuditService(db, audit);

    uavlog::AppContext ctx{
        db, audit, authService, fleetService, flightService,
        preflightService, maintService, reportService, backupAuditService,
        backupsDir, exportsDir
    };

    // Режим автоматического снятия скриншотов окон для отчёта и документации
    const QStringList args = app.arguments();
    const int ssIdx = args.indexOf("--screenshot-dir");
    if (ssIdx != -1 && ssIdx + 1 < args.size()) {
        const QString outDir = args.at(ssIdx + 1);
        QDir().mkpath(outDir);
        const auto loginRes = authService.authenticate("admin", "Admin1234!");
        if (loginRes.success && loginRes.user.has_value()) {
            uavlog::MainWindow w(ctx, *loginRes.user);
            w.show();
            app.processEvents();
            w.grab().save(QDir(outDir).filePath("app-01-mainwindow.png"));

            uavlog::FlightDialog dlgOk(ctx, *loginRes.user, &w);
            dlgOk.selectUavByIndex(0);      // БЛА-01 (исправен, осмотр пройден)
            dlgOk.selectOperatorByIndex(2); // Соколов Д. С. (действующий допуск)
            dlgOk.show();
            app.processEvents();
            dlgOk.grab().save(QDir(outDir).filePath("app-02-flight-allowed.png"));

            uavlog::FlightDialog dlgDeny(ctx, *loginRes.user, &w);
            dlgDeny.selectUavByIndex(1);      // БЛА-02 (критичные замечания F-15)
            dlgDeny.selectOperatorByIndex(0); // Громов В. К. (допуск истёк)
            dlgDeny.show();
            app.processEvents();
            dlgDeny.grab().save(QDir(outDir).filePath("app-03-flight-denied.png"));

            uavlog::PreflightDialog dlgPre(ctx, *loginRes.user, 1, QDate::currentDate(), &w);
            dlgPre.show();
            app.processEvents();
            dlgPre.grab().save(QDir(outDir).filePath("app-04-preflight-f15.png"));

            uavlog::UavFormularDialog dlgForm(ctx, *loginRes.user, 1, &w);
            dlgForm.show();
            app.processEvents();
            dlgForm.grab().save(QDir(outDir).filePath("app-05-formular.png"));

            uavlog::ReportsDialog dlgRep(ctx, *loginRes.user, uavlog::ReportKind::FleetReadinessAct, &w);
            dlgRep.show();
            app.processEvents();
            dlgRep.grab().save(QDir(outDir).filePath("app-06-reports-readiness.png"));
        }
        db.closeCleanly();
        return 0;
    }

    uavlog::LoginDialog loginDlg(authService);
    if (loginDlg.exec() != QDialog::Accepted || !loginDlg.authenticatedUser().has_value()) {
        db.closeCleanly();
        return 0;
    }

    uavlog::MainWindow mainWindow(ctx, *loginDlg.authenticatedUser());
    mainWindow.show();
    const int exitCode = app.exec();

    // REQ-REL-002: автоматическая резервная копия при выходе (раз в 24 ч, ротация 30 копий FIFO)
    backupAuditService.performAutoBackupIfNeeded(backupsDir, false);
    db.closeCleanly();
    return exitCode;
}
