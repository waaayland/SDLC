#include "infra/CryptoProvider.h"

#include <QCryptographicHash>
#include <QMessageAuthenticationCode>
#include <QRandomGenerator>

namespace uavlog {

bool CryptoProvider::isPasswordPolicySatisfied(const QString& password) {
    return password.size() >= kMinPasswordLength;
}

QString CryptoProvider::generateSaltHex() {
    QByteArray salt(kSaltBytes, Qt::Uninitialized);
    auto* rng = QRandomGenerator::system();
    for (int i = 0; i < kSaltBytes; ++i) {
        salt[i] = static_cast<char>(rng->bounded(256));
    }
    return QString::fromLatin1(salt.toHex());
}

QByteArray CryptoProvider::deriveKeyPbkdf2Sha256(const QByteArray& password,
                                                const QByteArray& salt,
                                                int iterations,
                                                int derivedKeyBytes) {
    if (iterations <= 0 || derivedKeyBytes <= 0) {
        return {};
    }

    constexpr int hLen = 32; // SHA-256 output size
    const int blockCount = (derivedKeyBytes + hLen - 1) / hLen;
    QByteArray dk;
    dk.reserve(blockCount * hLen);

    for (int block = 1; block <= blockCount; ++block) {
        QByteArray saltAndIndex = salt;
        saltAndIndex.append(static_cast<char>((block >> 24) & 0xFF));
        saltAndIndex.append(static_cast<char>((block >> 16) & 0xFF));
        saltAndIndex.append(static_cast<char>((block >> 8) & 0xFF));
        saltAndIndex.append(static_cast<char>(block & 0xFF));

        QByteArray u = QMessageAuthenticationCode::hash(
            saltAndIndex, password, QCryptographicHash::Sha256);
        QByteArray t = u;

        for (int iter = 1; iter < iterations; ++iter) {
            u = QMessageAuthenticationCode::hash(u, password, QCryptographicHash::Sha256);
            for (int k = 0; k < hLen; ++k) {
                t[k] = static_cast<char>(t[k] ^ u[k]);
            }
        }
        dk.append(t);
    }

    dk.truncate(derivedKeyBytes);
    return dk;
}

QString CryptoProvider::hashPasswordPbkdf2(const QString& password,
                                           const QString& saltHex,
                                           int iterations) {
    const QByteArray saltBytes = QByteArray::fromHex(saltHex.toLatin1());
    const QByteArray dk = deriveKeyPbkdf2Sha256(password.toUtf8(), saltBytes, iterations, 32);
    return QString::fromLatin1(dk.toHex());
}

bool CryptoProvider::verifyPassword(const QString& password,
                                    const QString& saltHex,
                                    const QString& expectedHashHex,
                                    int iterations) {
    const QByteArray actual = hashPasswordPbkdf2(password, saltHex, iterations).toLatin1();
    const QByteArray expected = expectedHashHex.toLatin1();
    if (actual.size() != expected.size() || actual.isEmpty()) {
        return false;
    }
    // Сравнение за постоянное время (защита от timing-атак, ADR-0003)
    unsigned char diff = 0;
    for (int i = 0; i < actual.size(); ++i) {
        diff |= static_cast<unsigned char>(actual[i] ^ expected[i]);
    }
    return diff == 0;
}

} // namespace uavlog
