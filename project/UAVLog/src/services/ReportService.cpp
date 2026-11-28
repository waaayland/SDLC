#include "services/ReportService.h"
#include "infra/CsvExporter.h"
#include "infra/PdfExporter.h"

#include <QDateTime>
#include <QSqlQuery>

namespace uavlog {

ReportService::ReportService(DatabaseManager& db, FleetService& fleet, PreflightService& preflight)
    : m_db(db), m_fleet(fleet), m_preflight(preflight) {}

QString ReportService::formatMinutes(int totalMin) {
    return QString("%1:%2").arg(totalMin / 60).arg(totalMin % 60, 2, 10, QChar('0'));
}

QString ReportService::formatDateRu(const QString& isoDate) {
    const QDate d = QDate::fromString(isoDate, Qt::ISODate);
    return d.isValid() ? d.toString("dd.MM.yyyy") : (isoDate.isEmpty() ? "—" : isoDate);
}

QString ReportService::buildHtmlTableDocument(const QString& title,
                                              const QString& periodText,
                                              const QString& generatedAtText,
                                              const QStringList& headers,
                                              const std::vector<QStringList>& rows,
                                              const QStringList& totalsRow,
                                              const QString& signerLine) {
    QString html;
    html += R"(<!DOCTYPE html><html><head><meta charset="utf-8"><style>
        body { font-family: 'DejaVu Sans', 'Segoe UI', Arial, sans-serif; font-size: 9pt; color: #111; }
        h1 { font-size: 13pt; margin: 0 0 4px 0; text-align: center; }
        .meta { font-size: 8.5pt; color: #333; margin-bottom: 10px; text-align: center; }
        table { width: 100%; border-collapse: collapse; margin-top: 6px; }
        th, td { border: 1px solid #444; padding: 4px 6px; text-align: left; }
        thead { display: table-header-group; }
        th { background-color: #E8EEF5; font-weight: bold; }
        tr.totals td { background-color: #F2F4F7; font-weight: bold; }
        .sign { margin-top: 26px; font-size: 9.5pt; }
    </style></head><body>)";

    html += QString("<h1>%1</h1>").arg(title.toHtmlEscaped());
    html += QString("<div class='meta'>ПО «UAVLog 1.0» | %1 | Дата формирования: %2</div>")
                .arg(periodText.toHtmlEscaped(), generatedAtText.toHtmlEscaped());

    html += "<table><thead><tr>";
    for (const auto& h : headers) {
        html += QString("<th>%1</th>").arg(h.toHtmlEscaped());
    }
    html += "</tr></thead><tbody>";

    if (rows.empty()) {
        html += QString("<tr><td colspan='%1' style='text-align:center;color:#555;'>Данные за выбранный период отсутствуют</td></tr>")
                    .arg(headers.size());
    } else {
        for (const auto& r : rows) {
            html += "<tr>";
            for (const auto& cell : r) {
                html += QString("<td>%1</td>").arg(cell.toHtmlEscaped());
            }
            html += "</tr>";
        }
    }

    if (!totalsRow.isEmpty()) {
        html += "<tr class='totals'>";
        for (const auto& c : totalsRow) {
            html += QString("<td>%1</td>").arg(c.toHtmlEscaped());
        }
        html += "</tr>";
    }

    html += "</tbody></table>";
    html += QString("<div class='sign'>%1 _______________________ / _____________________ / &nbsp;&nbsp; Дата: «___» __________ 20___ г.</div>")
                .arg(signerLine.toHtmlEscaped());
    html += "</body></html>";
    return html;
}

ReportData ReportService::generateReport(ReportKind kind,
                                         const QDate& fromDate,
                                         const QDate& toDate,
                                         int uavFilterId,
                                         int operatorFilterId,
                                         const QString& signerName) {
    ReportData rep;
    rep.kind = kind;
    rep.generatedAtText = QDateTime::currentDateTime().toString("dd.MM.yyyy HH:mm");
    const QString fromIso = fromDate.toString(Qt::ISODate);
    const QString toIso = toDate.toString(Qt::ISODate);
    rep.periodText = QString("Период: с %1 по %2").arg(fromDate.toString("dd.MM.yyyy"), toDate.toString("dd.MM.yyyy"));

    QSqlQuery q(m_db.database());

    if (kind == ReportKind::FlightHoursByUav) {
        rep.title = "Сводный отчёт «Налёт по бортам БЛА»";
        rep.headers = {"Борт", "Тип БЛА", "Состояние", "Число полётов", "Налёт за период (ч:мм)", "Последний полёт"};

        QString sql = R"(
            SELECT u.tail_number, t.name, u.state,
                   COUNT(fl.id) AS cnt,
                   COALESCE(SUM(fl.duration_min), 0) AS total_min,
                   COALESCE(MAX(fl.flight_date), '') AS last_dt
            FROM uavs u
            JOIN uav_types t ON t.id = u.uav_type_id
            LEFT JOIN flights fl ON fl.uav_id = u.id
                 AND fl.flight_date >= :dfrom AND fl.flight_date <= :dto
            WHERE 1=1
        )";
        if (uavFilterId > 0) sql += " AND u.id = :uid";
        sql += " GROUP BY u.id ORDER BY u.tail_number;";

        q.prepare(sql);
        q.bindValue(":dfrom", fromIso);
        q.bindValue(":dto", toIso);
        if (uavFilterId > 0) q.bindValue(":uid", uavFilterId);
        q.exec();

        int sumFlights = 0;
        int sumMin = 0;
        while (q.next()) {
            const int cnt = q.value(3).toInt();
            const int mins = q.value(4).toInt();
            sumFlights += cnt;
            sumMin += mins;
            rep.rows.push_back({
                q.value(0).toString(),
                q.value(1).toString(),
                QString::fromStdString(toRussianStatus(fromDbStatus(q.value(2).toString().toStdString()))),
                QString::number(cnt),
                formatMinutes(mins),
                formatDateRu(q.value(5).toString())
            });
        }
        rep.totalsRow = {"ИТОГО", "", "", QString::number(sumFlights), formatMinutes(sumMin), ""};
    } else if (kind == ReportKind::FlightHoursByOperator) {
        rep.title = "Сводный отчёт «Налёт по операторам БЛА»";
        rep.headers = {"Оператор", "Должность", "Допуск до", "Число полётов", "Налёт за период (ч:мм)"};

        QString sql = R"(
            SELECT op.full_name, op.position, op.permit_expiry,
                   COUNT(fl.id) AS cnt,
                   COALESCE(SUM(fl.duration_min), 0) AS total_min
            FROM operators op
            LEFT JOIN flights fl ON fl.operator_id = op.id
                 AND fl.flight_date >= :dfrom AND fl.flight_date <= :dto
            WHERE 1=1
        )";
        if (operatorFilterId > 0) sql += " AND op.id = :oid";
        sql += " GROUP BY op.id ORDER BY op.full_name;";

        q.prepare(sql);
        q.bindValue(":dfrom", fromIso);
        q.bindValue(":dto", toIso);
        if (operatorFilterId > 0) q.bindValue(":oid", operatorFilterId);
        q.exec();

        int sumFlights = 0;
        int sumMin = 0;
        while (q.next()) {
            const int cnt = q.value(3).toInt();
            const int mins = q.value(4).toInt();
            sumFlights += cnt;
            sumMin += mins;
            rep.rows.push_back({
                q.value(0).toString(),
                q.value(1).toString(),
                formatDateRu(q.value(2).toString()),
                QString::number(cnt),
                formatMinutes(mins)
            });
        }
        rep.totalsRow = {"ИТОГО", "", "", QString::number(sumFlights), formatMinutes(sumMin)};
    } else if (kind == ReportKind::FaultsSummary) {
        rep.title = "Отчёт «Неисправности и отказы парка БЛА»";
        rep.headers = {"Дата", "Борт", "Критичность", "Описание неисправности", "Статус / Устранение"};

        QString sql = R"(
            SELECT f.reported_date, u.tail_number, f.is_critical, f.description,
                   COALESCE(f.resolved_date, ''), COALESCE(f.resolution_works, '')
            FROM faults f
            JOIN uavs u ON u.id = f.uav_id
            WHERE f.reported_date >= :dfrom AND f.reported_date <= :dto
        )";
        if (uavFilterId > 0) sql += " AND f.uav_id = :uid";
        sql += " ORDER BY f.reported_date DESC, f.id DESC;";

        q.prepare(sql);
        q.bindValue(":dfrom", fromIso);
        q.bindValue(":dto", toIso);
        if (uavFilterId > 0) q.bindValue(":uid", uavFilterId);
        q.exec();

        int totalCount = 0;
        int openCrit = 0;
        while (q.next()) {
            ++totalCount;
            const bool crit = q.value(2).toInt() != 0;
            const QString resDate = q.value(4).toString();
            const QString resWorks = q.value(5).toString();
            if (crit && resDate.isEmpty()) ++openCrit;
            rep.rows.push_back({
                formatDateRu(q.value(0).toString()),
                q.value(1).toString(),
                crit ? "Критичная" : "Некритичная",
                q.value(3).toString(),
                resDate.isEmpty() ? "Открыта" : ("Устранена " + formatDateRu(resDate) + ": " + resWorks)
            });
        }
        rep.totalsRow = {"ИТОГО", QString("Записей: %1").arg(totalCount),
                         QString("Открытых крит.: %1").arg(openCrit), "", ""};
    } else if (kind == ReportKind::MaintenanceExecution) {
        rep.title = "Отчёт «Выполнение технического обслуживания БЛА»";
        rep.headers = {"Дата ТО", "Борт", "Вид ТО", "Налёт на момент ТО", "Исполнитель", "Примечание"};

        QString sql = R"(
            SELECT mr.performed_date, u.tail_number, r.kind_name,
                   mr.flight_min_at_maint, mr.performer_name, COALESCE(mr.notes, '')
            FROM maintenance_records mr
            JOIN uavs u ON u.id = mr.uav_id
            JOIN maintenance_rules r ON r.id = mr.rule_id
            WHERE mr.performed_date >= :dfrom AND mr.performed_date <= :dto
        )";
        if (uavFilterId > 0) sql += " AND mr.uav_id = :uid";
        sql += " ORDER BY mr.performed_date DESC, mr.id DESC;";

        q.prepare(sql);
        q.bindValue(":dfrom", fromIso);
        q.bindValue(":dto", toIso);
        if (uavFilterId > 0) q.bindValue(":uid", uavFilterId);
        q.exec();

        int count = 0;
        while (q.next()) {
            ++count;
            rep.rows.push_back({
                formatDateRu(q.value(0).toString()),
                q.value(1).toString(),
                q.value(2).toString(),
                formatMinutes(q.value(3).toInt()),
                q.value(4).toString(),
                q.value(5).toString()
            });
        }
        rep.totalsRow = {"ИТОГО", QString("Выполнено ТО: %1").arg(count), "", "", "", ""};
    } else {
        // Акт готовности парка БЛА к полётам (модуль F-15, T-15.4, REQ-FUNC-017)
        rep.title = "АКТ ГОТОВНОСТИ ПАРКА БЛА К ПОЛЁТАМ";
        rep.periodText = "Дата лётной смены: " + toDate.toString("dd.MM.yyyy");
        rep.headers = {"Борт", "Тип БЛА", "Состояние борта", "Предполётный осмотр", "Проверил", "Замечания", "Решение о допуске"};

        const auto readiness = m_preflight.buildFleetReadinessRows(toDate);
        int readyCount = 0;
        for (const auto& r : readiness) {
            if (uavFilterId > 0 && r.uavId != uavFilterId) continue;
            if (r.readyForFlight) ++readyCount;
            rep.rows.push_back({
                r.tailNumber,
                r.uavType,
                r.uavStatusRu,
                r.preflightStatusRu,
                r.inspectorName,
                r.defectsSummary,
                r.readyForFlight ? "ДОПУЩЕН К ВЫЛЕТУ" : "НЕ ДОПУЩЕН"
            });
        }
        rep.totalsRow = {
            "ИТОГО",
            QString("Активных бортов: %1").arg(rep.rows.size()),
            "", "", "", "",
            QString("Готово к вылету: %1").arg(readyCount)
        };
    }

    rep.htmlDocument = buildHtmlTableDocument(
        rep.title, rep.periodText, rep.generatedAtText,
        rep.headers, rep.rows, rep.totalsRow, signerName);
    return rep;
}

QString ReportService::buildFormularHtml(int uavId, const QDate& today) {
    const auto f = m_fleet.loadFormular(uavId, today);
    QString html;
    html += R"(<!DOCTYPE html><html><head><meta charset="utf-8"><style>
        body { font-family: 'DejaVu Sans', 'Segoe UI', Arial, sans-serif; font-size: 9pt; color: #111; }
        h1 { font-size: 13pt; margin: 0 0 4px 0; text-align: center; }
        h2 { font-size: 10.5pt; margin: 12px 0 4px 0; border-bottom: 1px solid #2B5B84; padding-bottom: 2px; }
        .meta { font-size: 8.5pt; color: #333; margin-bottom: 10px; text-align: center; }
        table { width: 100%; border-collapse: collapse; margin-top: 4px; }
        th, td { border: 1px solid #444; padding: 3px 6px; text-align: left; }
        thead { display: table-header-group; }
        th { background-color: #E8EEF5; }
        .sign { margin-top: 24px; font-size: 9.5pt; }
    </style></head><body>)";

    html += QString("<h1>ЭЛЕКТРОННЫЙ ФОРМУЛЯР БЛА — %1</h1>")
                .arg(QString::fromStdString(f.uav.tailNumber).toHtmlEscaped());
    html += QString("<div class='meta'>ПО «UAVLog 1.0» | Сформирован: %1</div>")
                .arg(QDateTime::currentDateTime().toString("dd.MM.yyyy HH:mm"));

    html += "<h2>1. Паспортные данные</h2><table>";
    html += QString("<tr><th>Бортовой номер</th><td>%1</td><th>Тип БЛА</th><td>%2</td></tr>")
                .arg(QString::fromStdString(f.uav.tailNumber).toHtmlEscaped(),
                     QString::fromStdString(f.uav.typeName).toHtmlEscaped());
    html += QString("<tr><th>Заводской номер</th><td>%1</td><th>Дата ввода</th><td>%2</td></tr>")
                .arg(QString::fromStdString(f.uav.serialNumber).toHtmlEscaped(),
                     formatDateRu(QString::fromStdString(f.uav.commissionDateIso)));
    html += QString("<tr><th>Состояние</th><td><b>%1</b></td><th>Суммарный налёт / полётов</th><td>%2 (%3 пол.)</td></tr>")
                .arg(QString::fromStdString(toRussianStatus(f.uav.status)),
                     formatMinutes(f.uav.totalFlightMin))
                .arg(f.uav.totalFlights);
    html += QString("<tr><th>Штатная полезная нагрузка</th><td colspan='3'>%1</td></tr>")
                .arg(QString::fromStdString(f.uav.standardPayload).toHtmlEscaped());
    html += "</table>";

    html += "<h2>2. Остатки до регламентного ТО</h2><table><thead><tr><th>Вид ТО</th><th>Остаток по налёту</th><th>Остаток по календарю</th><th>Статус</th></tr></thead><tbody>";
    for (const auto& r : f.remainders) {
        const QString hStr = r.remainingHours.has_value() ? QString("%1 ч").arg(*r.remainingHours, 0, 'f', 1) : "—";
        const QString dStr = r.remainingDays.has_value() ? QString("%1 дн.").arg(*r.remainingDays) : "—";
        const QString stStr = r.isOverdue ? "ПРОСРОЧЕНО" : (r.isWarning ? "Приближается срок" : "В норме");
        html += QString("<tr><td>%1</td><td>%2</td><td>%3</td><td>%4</td></tr>")
                    .arg(QString::fromStdString(r.ruleTitle).toHtmlEscaped(), hStr, dStr, stStr);
    }
    html += "</tbody></table>";

    html += "<h2>3. Журнал полётов</h2><table><thead><tr><th>Дата</th><th>Взлёт–Посадка</th><th>Длит.</th><th>Оператор</th><th>АКБ</th><th>Задача и район</th></tr></thead><tbody>";
    for (const auto& fl : f.flights) {
        html += QString("<tr><td>%1</td><td>%2–%3</td><td>%4</td><td>%5</td><td>%6</td><td>%7</td></tr>")
                    .arg(formatDateRu(QString::fromStdString(fl.flightDateIso)),
                         QString::fromStdString(fl.takeoffTime),
                         QString::fromStdString(fl.landingTime),
                         formatMinutes(fl.durationMinutes),
                         QString::fromStdString(fl.operatorName).toHtmlEscaped(),
                         QString::fromStdString(fl.batteryInventory).toHtmlEscaped(),
                         QString::fromStdString(fl.taskAndArea).toHtmlEscaped());
    }
    if (f.flights.empty()) html += "<tr><td colspan='6'>Полёты не зарегистрированы</td></tr>";
    html += "</tbody></table>";

    html += "<h2>4. Журнал выполненного ТО</h2><table><thead><tr><th>Дата</th><th>Вид ТО</th><th>Налёт на ТО</th><th>Исполнитель</th><th>Примечание</th></tr></thead><tbody>";
    for (const auto& m : f.maintRecords) {
        html += QString("<tr><td>%1</td><td>%2</td><td>%3</td><td>%4</td><td>%5</td></tr>")
                    .arg(formatDateRu(QString::fromStdString(m.performedDateIso)),
                         QString::fromStdString(m.ruleTitle).toHtmlEscaped(),
                         formatMinutes(m.flightMinAtMaint),
                         QString::fromStdString(m.performer).toHtmlEscaped(),
                         QString::fromStdString(m.notes).toHtmlEscaped());
    }
    if (f.maintRecords.empty()) html += "<tr><td colspan='5'>Записи ТО отсутствуют</td></tr>";
    html += "</tbody></table>";

    html += "<h2>5. Журнал неисправностей</h2><table><thead><tr><th>Дата</th><th>Критичность</th><th>Описание</th><th>Устранение</th></tr></thead><tbody>";
    for (const auto& flt : f.faults) {
        html += QString("<tr><td>%1</td><td>%2</td><td>%3</td><td>%4</td></tr>")
                    .arg(formatDateRu(QString::fromStdString(flt.reportedDateIso)),
                         flt.isCritical ? "Критичная" : "Некритичная",
                         QString::fromStdString(flt.description).toHtmlEscaped(),
                         flt.isOpen() ? "Открыта" : (formatDateRu(QString::fromStdString(flt.resolvedDateIso)) + ": " +
                                                     QString::fromStdString(flt.resolutionWorks).toHtmlEscaped()));
    }
    if (f.faults.empty()) html += "<tr><td colspan='4'>Неисправности отсутствуют</td></tr>";
    html += "</tbody></table>";

    html += "<div class='sign'>Начальник службы эксплуатации БЛА ___________________ / ___________________ / Дата: «___» ________ 20___ г.</div>";
    html += "</body></html>";
    return html;
}

bool ReportService::exportReportCsv(const ReportData& report, const QString& filePath, QString* errorOut) {
    auto rows = report.rows;
    if (!report.totalsRow.isEmpty()) {
        rows.push_back(report.totalsRow);
    }
    return CsvExporter::exportTable(filePath, report.headers, rows, errorOut);
}

bool ReportService::exportReportPdf(const ReportData& report, const QString& filePath, QString* errorOut) {
    return PdfExporter::exportHtmlToPdfA4(filePath, report.htmlDocument, report.title, errorOut);
}

bool ReportService::printReportA4(const ReportData& report, QWidget* parent, QString* errorOut) {
    return PdfExporter::printHtmlA4(report.htmlDocument, report.title, parent, errorOut);
}

bool ReportService::exportFormularPdf(int uavId, const QString& filePath, QString* errorOut) {
    const QString html = buildFormularHtml(uavId);
    return PdfExporter::exportHtmlToPdfA4(filePath, html, "Формуляр БЛА", errorOut);
}

bool ReportService::printFormularA4(int uavId, QWidget* parent, QString* errorOut) {
    const QString html = buildFormularHtml(uavId);
    return PdfExporter::printHtmlA4(html, "Формуляр БЛА", parent, errorOut);
}

} // namespace uavlog
