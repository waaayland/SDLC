#include "ui/ServiceDialogs.h"

#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace uavlog {

// ==================== AuditLogDialog ====================

AuditLogDialog::AuditLogDialog(AppContext& ctx, const UserAccount& actor, QWidget* parent)
    : QDialog(parent), m_ctx(ctx), m_actor(actor) {
    setWindowTitle("Журнал аудита изменений (только чтение — REQ-SEC-004)");
    setMinimumSize(900, 520);

    auto* root = new QVBoxLayout(this);

    auto* top = new QHBoxLayout();
    top->addWidget(new QLabel("Фильтр:", this));
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText("Действие, объект, логин или значение...");
    top->addWidget(m_searchEdit, 1);
    auto* csvBtn = new QPushButton("Экспорт CSV (UTF-8 BOM)", this);
    top->addWidget(csvBtn);
    root->addLayout(top);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(7);
    m_table->setHorizontalHeaderLabels({"ID", "Дата и время", "Пользователь", "Действие", "Объект", "Было", "Стало"});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers); // Неизменяемый журнал (REQ-SEC-004)
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setAlternatingRowColors(true);
    root->addWidget(m_table, 1);

    auto* btns = new QHBoxLayout();
    btns->addWidget(new QLabel("<i>Редактирование и удаление записей аудита через GUI запрещены (REQ-SEC-004).</i>", this));
    btns->addStretch();
    auto* closeBtn = new QPushButton("Закрыть", this);
    btns->addWidget(closeBtn);
    root->addLayout(btns);

    connect(m_searchEdit, &QLineEdit::textChanged, this, &AuditLogDialog::reloadAudit);
    connect(csvBtn, &QPushButton::clicked, this, &AuditLogDialog::onExportCsvClicked);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);

    reloadAudit();
}

void AuditLogDialog::reloadAudit() {
    QString err;
    const auto rows = m_ctx.backupAudit.loadAuditEntries(m_actor, m_searchEdit->text(), &err);
    m_table->setRowCount(static_cast<int>(rows.size()));
    for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
        const auto& e = rows[i];
        m_table->setItem(i, 0, new QTableWidgetItem(QString::number(e.id)));
        m_table->setItem(i, 1, new QTableWidgetItem(QString::fromStdString(e.createdAtIso)));
        m_table->setItem(i, 2, new QTableWidgetItem(QString::fromStdString(e.username)));
        m_table->setItem(i, 3, new QTableWidgetItem(QString::fromStdString(e.action)));
        m_table->setItem(i, 4, new QTableWidgetItem(
            QString("%1 #%2").arg(QString::fromStdString(e.entityType)).arg(e.entityId)));
        m_table->setItem(i, 5, new QTableWidgetItem(QString::fromStdString(e.oldValue)));
        m_table->setItem(i, 6, new QTableWidgetItem(QString::fromStdString(e.newValue)));
    }
}

void AuditLogDialog::onExportCsvClicked() {
    QDir().mkpath(m_ctx.exportsDir);
    const QString defName = QDir(m_ctx.exportsDir).filePath(
        QString("audit_log_%1.csv").arg(QDate::currentDate().toString("yyyyMMdd")));
    const QString path = QFileDialog::getSaveFileName(this, "Экспорт аудита в CSV", defName, "CSV (*.csv)");
    if (path.isEmpty()) return;

    QString err;
    if (!m_ctx.backupAudit.exportAuditCsv(m_actor, path, m_searchEdit->text(), &err)) {
        QMessageBox::warning(this, "Ошибка экспорта", err);
        return;
    }
    QMessageBox::information(this, "Экспорт завершён", "Журнал аудита экспортирован в CSV:\n" + path);
}

// ==================== BackupRestoreDialog ====================

BackupRestoreDialog::BackupRestoreDialog(AppContext& ctx, const UserAccount& actor, QWidget* parent)
    : QDialog(parent), m_ctx(ctx), m_actor(actor) {
    setWindowTitle("Резервное копирование и восстановление БД (VACUUM INTO)");
    setMinimumSize(680, 440);

    auto* root = new QVBoxLayout(this);
    root->addWidget(new QLabel(
        QString("<b>Текущая БД SQLite 3 (WAL):</b> %1<br/>"
                "<b>Каталог резервных копий (автокопии каждые 24 ч, ротация 30 копий):</b> %2")
            .arg(m_ctx.db.dbPath().toHtmlEscaped(), m_ctx.backupsDir.toHtmlEscaped()),
        this));

    m_list = new QListWidget(this);
    root->addWidget(m_list, 1);

    auto* btns = new QHBoxLayout();
    auto* createBtn = new QPushButton("Создать копию сейчас...", this);
    auto* restoreSelBtn = new QPushButton("Восстановить выбранную", this);
    auto* restoreFileBtn = new QPushButton("Восстановить из файла...", this);
    auto* closeBtn = new QPushButton("Закрыть", this);

    btns->addWidget(createBtn);
    btns->addWidget(restoreSelBtn);
    btns->addWidget(restoreFileBtn);
    btns->addStretch();
    btns->addWidget(closeBtn);
    root->addLayout(btns);

    connect(createBtn, &QPushButton::clicked, this, &BackupRestoreDialog::onCreateBackupClicked);
    connect(restoreSelBtn, &QPushButton::clicked, this, &BackupRestoreDialog::onRestoreSelectedClicked);
    connect(restoreFileBtn, &QPushButton::clicked, this, &BackupRestoreDialog::onRestoreFromFileClicked);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);

    reloadBackupList();
}

void BackupRestoreDialog::reloadBackupList() {
    m_list->clear();
    const auto files = BackupAuditService::listBackupFiles(m_ctx.backupsDir);
    for (const auto& f : files) {
        const QFileInfo fi(f);
        auto* item = new QListWidgetItem(
            QString("%1  (%2 КБ | %3)")
                .arg(fi.fileName())
                .arg(qMax<qint64>(1, fi.size() / 1024))
                .arg(fi.lastModified().toString("dd.MM.yyyy HH:mm:ss")));
        item->setData(Qt::UserRole, f);
        m_list->addItem(item);
    }
}

void BackupRestoreDialog::onCreateBackupClicked() {
    QDir().mkpath(m_ctx.backupsDir);
    const QString defName = QDir(m_ctx.backupsDir).filePath(
        QString("uavlog_manual_%1.db").arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss")));
    const QString path = QFileDialog::getSaveFileName(this, "Сохранить резервную копию БД", defName, "SQLite DB (*.db)");
    if (path.isEmpty()) return;

    QString err;
    if (!m_ctx.backupAudit.createManualBackup(m_actor, path, &err)) {
        QMessageBox::warning(this, "Ошибка резервного копирования", err);
        return;
    }
    reloadBackupList();
    QMessageBox::information(this, "Копия создана",
                             "Согласованная горячая резервная копия (VACUUM INTO) успешно создана:\n" + path);
}

void BackupRestoreDialog::onRestoreSelectedClicked() {
    auto* item = m_list->currentItem();
    if (!item) {
        QMessageBox::information(this, "Выбор копии", "Выберите резервную копию из списка.");
        return;
    }
    doRestore(item->data(Qt::UserRole).toString());
}

void BackupRestoreDialog::onRestoreFromFileClicked() {
    const QString path = QFileDialog::getOpenFileName(this, "Выберите файл резервной копии БД",
                                                      m_ctx.backupsDir, "SQLite DB (*.db)");
    if (path.isEmpty()) return;
    doRestore(path);
}

void BackupRestoreDialog::doRestore(const QString& backupPath) {
    const auto ans = QMessageBox::question(
        this, "Подтверждение восстановления БД",
        "Восстановить базу данных из файла:\n" + backupPath +
            "\n\nПеред восстановлением автоматически будет создана страховая копия текущей БД (REQ-FUNC-015).",
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (ans != QMessageBox::Yes) return;

    QString safetyPath;
    QString err;
    if (!m_ctx.backupAudit.restoreFromBackup(m_actor, backupPath, m_ctx.backupsDir, &safetyPath, &err)) {
        QMessageBox::critical(this, "Ошибка восстановления", err);
        return;
    }
    m_restored = true;
    reloadBackupList();
    QMessageBox::information(
        this, "БД восстановлена",
        "База данных успешно восстановлена из копии.\nСтраховая копия предыдущего состояния сохранена в:\n" + safetyPath);
}

} // namespace uavlog
