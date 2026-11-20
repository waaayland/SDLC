#include "infra/CryptoProvider.h"

#include <QCryptographicHash>
#include <QMessageAuthenticationCode>
#include <QRandomGenerator>

namespace uavlog {

QString CryptoProvider::generateSaltHex(int bytes) {
    QByteArray salt;
    salt.resize(bytes);
    auto* rng = QRandomGenerator::system();
    for (int i = 0; i < bytes; ++i) {
        salt[i] = static_cast<char>(rng->bounded(256));
    }
    return QString::fromLatin1(salt.toHex());
}

QString CryptoProvider::hashPasswordPbkdf2(const QString& password,
                                           const QString& saltHex,
                                           int iterations) {
    // Стандартная реализация RFC 2898 PBKDF2-HMAC-SHA256 (32 байта / 1 блок) на Qt6::Core
    const QByteArray key = password.toUtf8();
    QByteArray saltBlock = QByteArray::fromHex(saltHex.toLatin1());
    // Big-endian индекс блока = 1
    saltBlock.append('\x00');
    saltBlock.append('\x00');
    saltBlock.append('\x00');
    saltBlock.append('\x01');

    QByteArray u = QMessageAuthenticationCode::hash(saltBlock, key, QCryptographicHash::Sha256);
    QByteArray derived = u;
    for (int i = 1; i < iterations; ++i) {
        u = QMessageAuthenticationCode::hash(u, key, QCryptographicHash::Sha256);
        for (int j = 0; j < derived.size(); ++j) {
            derived[j] = static_cast<char>(derived[j] ^ u[j]);
        }
    }
    return QString::fromLatin1(derived.toHex());
}

bool CryptoProvider::verifyPassword(const QString& password,
                                    const QString& saltHex,
                                    const QString& expectedHashHex,
                                    int iterations) {
    const QByteArray actual = QByteArray::fromHex(
        hashPasswordPbkdf2(password, saltHex, iterations).toLatin1());
    const QByteArray expected = QByteArray::fromHex(expectedHashHex.toLatin1());
    if (actual.size() != expected.size() || actual.isEmpty()) {
        return false;
    }
    // Сравнение за постоянное время (защита от timing-атак)
    unsigned char diff = 0;
    for (int i = 0; i < actual.size(); ++i) {
        diff |= static_cast<unsigned char>(actual[i] ^ expected[i]);
    }
    return diff == 0;
}

bool CryptoProvider::isPasswordPolicySatisfied(const QString& password) {
    return password.size() >= kMinPasswordLength;
}

} // namespace uavlog
