#include "infra/PdfExporter.h"

#include <QDir>
#include <QFileInfo>
#include <QMarginsF>
#include <QPageLayout>
#include <QPageSize>
#include <QPdfWriter>
#include <QPrintDialog>
#include <QPrinter>
#include <QTextDocument>

namespace uavlog {

bool PdfExporter::exportHtmlToPdfA4(const QString& filePath,
                                    const QString& htmlContent,
                                    const QString& documentTitle,
                                    QString* errorOut) {
    QDir().mkpath(QFileInfo(filePath).absolutePath());

    QPdfWriter writer(filePath);
    writer.setTitle(documentTitle);
    writer.setCreator("UAVLog 1.0");
    writer.setPageSize(QPageSize(QPageSize::A4));
    writer.setPageOrientation(QPageLayout::Portrait);
    writer.setPageMargins(QMarginsF(15, 15, 15, 15), QPageLayout::Millimeter);
    writer.setResolution(150);

    QTextDocument doc;
    doc.setHtml(htmlContent);
    doc.setPageSize(QSizeF(writer.width(), writer.height()));
    doc.print(&writer);

    if (!QFileInfo::exists(filePath) || QFileInfo(filePath).size() == 0) {
        if (errorOut) *errorOut = "Не удалось сформировать PDF-файл: " + filePath;
        return false;
    }
    return true;
}

bool PdfExporter::printHtmlA4(const QString& htmlContent,
                              const QString& documentTitle,
                              QWidget* parent,
                              QString* errorOut) {
    QPrinter printer(QPrinter::HighResolution);
    printer.setDocName(documentTitle);
    printer.setPageSize(QPageSize(QPageSize::A4));
    printer.setPageOrientation(QPageLayout::Portrait);
    printer.setPageMargins(QMarginsF(15, 15, 15, 15), QPageLayout::Millimeter);

    QPrintDialog dlg(&printer, parent);
    dlg.setWindowTitle("Печать документа — " + documentTitle);
    if (dlg.exec() != QDialog::Accepted) {
        return false;
    }

    QTextDocument doc;
    doc.setHtml(htmlContent);
    doc.print(&printer);
    if (printer.printerState() == QPrinter::Error) {
        if (errorOut) *errorOut = "Ошибка спулера печати Windows при печати документа";
        return false;
    }
    return true;
}

} // namespace uavlog
