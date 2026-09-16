#ifndef PASSWORDSETTINGS_H
#define PASSWORDSETTINGS_H

#include <QString>
#include <QStringList>

/*!
 * \brief Проверенные (валидные) параметры генерации паролей.
 *
 * Часть слоя Model. Структура создаётся только классом PasswordModel после
 * успешной проверки введённых пользователем данных, поэтому любой её экземпляр
 * заведомо корректен.
 */
struct PasswordSettings
{
    int  length        = 12;    //!< длина одного пароля, символов
    int  count         = 5;     //!< сколько паролей нужно сгенерировать
    bool useLowercase  = true;  //!< включать строчные латинские буквы
    bool useUppercase  = true;  //!< включать прописные латинские буквы
    bool useDigits     = true;  //!< включать цифры
    bool useSpecial    = false; //!< включать специальные символы

    //! Количество выбранных наборов символов.
    int enabledSetCount() const
    {
        return (useLowercase ? 1 : 0) + (useUppercase ? 1 : 0)
             + (useDigits ? 1 : 0) + (useSpecial ? 1 : 0);
    }

    //! Названия выбранных наборов — для вывода на главной форме.
    QStringList enabledSetNames() const
    {
        QStringList names;
        if (useLowercase) names << QStringLiteral("строчные (a–z)");
        if (useUppercase) names << QStringLiteral("прописные (A–Z)");
        if (useDigits)    names << QStringLiteral("цифры (0–9)");
        if (useSpecial)   names << QStringLiteral("специальные (!@#…)");
        return names;
    }
};

#endif // PASSWORDSETTINGS_H
