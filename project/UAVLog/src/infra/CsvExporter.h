#pragma once

#include <QString>
#include <QStringList>
#include <vector>

namespace uavlog {

// Потоковый экспорт CSV: UTF-8 с BOM, разделитель «;» (REQ-INT-007, ADR-0005)
class CsvExporter {
public:
    static bool exportTable(const QString& filePath,
                            const QStringList& headers,
                            const std::vector<QStringList>& rows,
                            QString* errorOut = nullptr);

private:
    static QString escapeField(const QString& field);
};

} // namespace uavlog
