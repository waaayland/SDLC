#ifndef RAWPASSWORDINPUT_H
#define RAWPASSWORDINPUT_H

#include <QString>

/*!
 * \brief «Сырые» данные, введённые пользователем в дополнительном окне.
 *
 * Числовые поля хранятся в виде строк намеренно: только так модель может
 * обнаружить некорректный ввод («abc», пустая строка, отрицательное число)
 * и выдать сообщение об ошибке, как того требует задание.
 *
 * Именно этот объект модель запоминает как «последние введённые данные»
 * и отдаёт обратно при повторном открытии окна ввода.
 */
struct RawPasswordInput
{
    QString lengthText  = QStringLiteral("12");
    QString countText   = QStringLiteral("5");
    bool    useLowercase = true;
    bool    useUppercase = true;
    bool    useDigits    = true;
    bool    useSpecial   = false;
};

#endif // RAWPASSWORDINPUT_H
