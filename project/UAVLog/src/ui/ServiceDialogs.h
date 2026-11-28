#pragma once

#include "core/DomainTypes.h"
#include "ui/AppContext.h"

#include <QDialog>

class QLineEdit;
class QListWidget;
class QTableWidget;

namespace uavlog {

// Просмотр неизменяемого журнала аудита + CSV (модуль F-13, REQ-SEC-004, ADR-0003)
class AuditLogDialog : public QDialog {
    Q_OBJECT
public:
    AuditLogDialog(AppContext& ctx, const UserAccount& actor, QWidget* parent = nullptr);

private slots:
    void reloadAudit();
    void onExportCsvClicked();

private:
    AppContext& m_ctx;
    UserAccount m_actor;
    QLineEdit* m_searchEdit = nullptr;
    QTableWidget* m_table = nullptr;
};

// Резервное копирование и восстановление БД SQLite (модуль F-14, REQ-FUNC-015, REQ-REL-002)
class BackupRestoreDialog : public QDialog {
    Q_OBJECT
public:
    BackupRestoreDialog(AppContext& ctx, const UserAccount& actor, QWidget* parent = nullptr);

    bool databaseRestored() const { return m_restored; }

private slots:
    void reloadBackupList();
    void onCreateBackupClicked();
    void onRestoreSelectedClicked();
    void onRestoreFromFileClicked();

private:
    void doRestore(const QString& backupPath);

    AppContext& m_ctx;
    UserAccount m_actor;
    bool m_restored = false;
    QListWidget* m_list = nullptr;
};

} // namespace uavlog
