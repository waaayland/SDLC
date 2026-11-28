#include "infra/CsvExporter.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace uavlog {

QString CsvExporter::escapeField(const QString& field) {
    QString clean = field;
    clean.replace("\r\n", " ");
    clean.replace('\n', ' ');
    if (clean.contains(';') || clean.contains('"') || clean.contains('\r')) {
        clean.replace('"', "\"\"");
        return '"' + clean + '"';
    }
    return clean;
}

bool CsvExporter::exportTable(const QString& filePath,
                              const QStringList& headers,
                              const std::vector<QStringList>& rows,
                              QString* errorOut) {
    QDir().mkpath(QFileInfo(filePath).absolutePath());
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorOut) *errorOut = "Не удалось открыть файл для записи: " + file.errorString();
        return false;
    }

    // Обязательный UTF-8 BOM для корректного открытия кириллицы двойным щелчком в Excel (REQ-INT-007)
    static const unsigned char kBom[3] = {0xEF, 0xBB, 0xBF};
    file.write(reinterpret_cast<const char*>(kBom), 3);

    auto writeRow = [&](const QStringList& cols) {
        QStringList escaped;
        escaped.reserve(cols.size());
        for (const auto& c : cols) {
            escaped << escapeField(c);
        }
        const QByteArray line = (escaped.join(';') + "\r\n").toUtf8();
        file.write(line);
    };

    writeRow(headers);
    for (const auto& row : rows) {
        writeRow(row);
    }

    file.flush();
    return true;
}

} // namespace uavlog
