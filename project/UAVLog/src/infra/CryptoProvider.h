#pragma once

#include <QByteArray>
#include <QString>

namespace uavlog {

// Криптопровайдер PBKDF2-HMAC-SHA256 без внешних библиотек (REQ-SEC-001, ADR-0003)
class CryptoProvider {
public:
    static constexpr int kDefaultIterations = 100000;
    static constexpr int kSaltBytes = 16;
    static constexpr int kMinPasswordLength = 8;

    static bool isPasswordPolicySatisfied(const QString& password);
    static QString generateSaltHex();

    // RFC 2898 PBKDF2-HMAC-SHA256 (сырой бинарный вывод, проверяется тест-векторами)
    static QByteArray deriveKeyPbkdf2Sha256(const QByteArray& password,
                                            const QByteArray& salt,
                                            int iterations,
                                            int derivedKeyBytes = 32);

    static QString hashPasswordPbkdf2(const QString& password,
                                      const QString& saltHex,
                                      int iterations = kDefaultIterations);

    static bool verifyPassword(const QString& password,
                               const QString& saltHex,
                               const QString& expectedHashHex,
                               int iterations = kDefaultIterations);
};

} // namespace uavlog
