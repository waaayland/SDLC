#include "infra/AuditLogger.h"
#include "infra/DatabaseManager.h"
#include "services/AuthService.h"
#include "services/FleetService.h"
#include "ui/FlightDialog.h"
#include "ui/LoginDialog.h"
#include "ui/MainWindow.h"

#include <QApplication>
#include <QDir>
#include <QMessageBox>
#include <QPixmap>
#include <QStandardPaths>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("UAVLog");
    app.setApplicationVersion("0.1.0");
    app.setStyle("Fusion");
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

    const QString dataDir = QDir(QCoreApplication::applicationDirPath()).filePath("data");
    const QString logDir  = QDir(QCoreApplication::applicationDirPath()).filePath("logs");
    const QString dbPath  = QDir(dataDir).filePath("uavlog.db");

    uavlog::AuditLogger audit(logDir);
    uavlog::DatabaseManager db;
    QString err;
    if (!db.openAndInitialize(dbPath, &err)) {
        audit.logError("ERROR", "main", "Ошибка открытия БД: " + err);
        QMessageBox::critical(nullptr, "Ошибка БД UAVLog",
                              "Не удалось открыть или проверить БД:\n" + err);
        return 1;
    }
    db.seedDemoDataIfEmpty();

    uavlog::AuthService authService(db, audit);
    uavlog::FleetService fleetService(db, audit);

    // Режим автоматического снятия скриншотов окон для документации и отчёта
    const QStringList args = app.arguments();
    const int ssIdx = args.indexOf("--screenshot-dir");
    if (ssIdx != -1 && ssIdx + 1 < args.size()) {
        const QString outDir = args.at(ssIdx + 1);
        QDir().mkpath(outDir);
        const auto loginRes = authService.authenticate("admin", "Admin1234!");
        if (loginRes.success && loginRes.user.has_value()) {
            uavlog::MainWindow w(authService, fleetService, *loginRes.user);
            w.show();
            app.processEvents();
            w.grab().save(QDir(outDir).filePath("06-uavlog-mainwindow.png"));

            uavlog::FlightDialog dlgOk(fleetService, *loginRes.user, &w);
            dlgOk.selectUavByIndex(0);      // БЛА-01 (исправен, осмотр пройден)
            dlgOk.selectOperatorByIndex(2); // Соколов Д. С. (действующий допуск)
            dlgOk.show();
            app.processEvents();
            dlgOk.grab().save(QDir(outDir).filePath("07-uavlog-flight-allowed.png"));

            uavlog::FlightDialog dlgDeny(fleetService, *loginRes.user, &w);
            dlgDeny.selectUavByIndex(1);      // БЛА-02 (критичные замечания F-15)
            dlgDeny.selectOperatorByIndex(0); // Громов В. К. (допуск истёк)
            dlgDeny.show();
            app.processEvents();
            dlgDeny.grab().save(QDir(outDir).filePath("08-uavlog-flight-denied.png"));
        }
        return 0;
    }

    uavlog::LoginDialog loginDlg(authService);
    if (loginDlg.exec() != QDialog::Accepted || !loginDlg.authenticatedUser().has_value()) {
        return 0;
    }

    uavlog::MainWindow mainWindow(authService, fleetService, *loginDlg.authenticatedUser());
    mainWindow.show();
    return app.exec();
}
