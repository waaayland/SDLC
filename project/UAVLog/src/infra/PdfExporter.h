#pragma once

#include <QString>
#include <QWidget>

namespace uavlog {

// Генерация печатных форм и PDF A4 через HTML/CSS + QTextDocument + QPdfWriter/QPrinter (ADR-0005)
class PdfExporter {
public:
    static bool exportHtmlToPdfA4(const QString& filePath,
                                  const QString& htmlContent,
                                  const QString& documentTitle,
                                  QString* errorOut = nullptr);

    static bool printHtmlA4(const QString& htmlContent,
                            const QString& documentTitle,
                            QWidget* parent = nullptr,
                            QString* errorOut = nullptr);
};

} // namespace uavlog
