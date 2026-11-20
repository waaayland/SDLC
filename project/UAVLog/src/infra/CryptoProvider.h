#pragma once

#include <QString>

namespace uavlog {

class CryptoProvider {
public:
    static constexpr int kMinPasswordLength = 8;
    static constexpr int kDefaultIterations = 100000; // REQ-SEC-001

    static QString generateSaltHex(int bytes = 16);
    static QString hashPasswordPbkdf2(const QString& password,
                                      const QString& saltHex,
                                      int iterations = kDefaultIterations);
    static bool verifyPassword(const QString& password,
                               const QString& saltHex,
                               const QString& expectedHashHex,
                               int iterations = kDefaultIterations);
    static bool isPasswordPolicySatisfied(const QString& password);
};

} // namespace uavlog
