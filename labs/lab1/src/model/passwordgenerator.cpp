#include "model/passwordgenerator.h"

#include <QRandomGenerator>

const QString PasswordGenerator::Lowercase = QStringLiteral("abcdefghijklmnopqrstuvwxyz");
const QString PasswordGenerator::Uppercase = QStringLiteral("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
const QString PasswordGenerator::Digits    = QStringLiteral("0123456789");
const QString PasswordGenerator::Special   = QStringLiteral("!@#$%^&*()-_=+[]{};:,.?/");

QStringList PasswordGenerator::enabledAlphabets(const PasswordSettings &settings)
{
    QStringList alphabets;
    if (settings.useLowercase) alphabets << Lowercase;
    if (settings.useUppercase) alphabets << Uppercase;
    if (settings.useDigits)    alphabets << Digits;
    if (settings.useSpecial)   alphabets << Special;
    return alphabets;
}

QString PasswordGenerator::combinedAlphabet(const PasswordSettings &settings)
{
    return enabledAlphabets(settings).join(QString());
}

QChar PasswordGenerator::randomChar(const QString &alphabet)
{
    const quint32 index = QRandomGenerator::system()->bounded(alphabet.size());
    return alphabet.at(static_cast<int>(index));
}

QString PasswordGenerator::generateOne(const PasswordSettings &settings)
{
    const QStringList alphabets = enabledAlphabets(settings);
    const QString     combined  = combinedAlphabet(settings);

    QString password;
    password.reserve(settings.length);

    // Сначала по одному символу из каждого выбранного набора — так пароль
    // гарантированно удовлетворяет заданным пользователем требованиям.
    for (const QString &alphabet : alphabets)
        password.append(randomChar(alphabet));

    // Остаток добираем из объединённого алфавита.
    while (password.size() < settings.length)
        password.append(randomChar(combined));

    // Перемешивание Фишера—Йетса, иначе первые символы всегда шли бы
    // в порядке следования наборов.
    for (int i = password.size() - 1; i > 0; --i) {
        const int j = static_cast<int>(QRandomGenerator::system()->bounded(i + 1));
        const QChar tmp = password[i];
        password[i] = password[j];
        password[j] = tmp;
    }

    return password;
}

QStringList PasswordGenerator::generate(const PasswordSettings &settings)
{
    QStringList passwords;
    passwords.reserve(settings.count);

    for (int i = 0; i < settings.count; ++i)
        passwords << generateOne(settings);

    return passwords;
}
