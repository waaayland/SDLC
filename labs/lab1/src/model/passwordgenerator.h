#ifndef PASSWORDGENERATOR_H
#define PASSWORDGENERATOR_H

#include <QString>
#include <QStringList>

#include "model/passwordsettings.h"

/*!
 * \brief Алгоритм генерации паролей.
 *
 * Вынесен из PasswordModel в отдельный класс, чтобы модель отвечала только
 * за состояние и оповещение наблюдателей, а бизнес-алгоритм можно было
 * тестировать и заменять независимо.
 */
class PasswordGenerator
{
public:
    //! Алфавиты отдельных наборов символов.
    static const QString Lowercase;
    static const QString Uppercase;
    static const QString Digits;
    static const QString Special;

    /*!
     * \brief Сформировать settings.count паролей длиной settings.length.
     *
     * В каждом пароле гарантированно присутствует хотя бы один символ из
     * каждого выбранного набора. Используется криптографический генератор
     * случайных чисел QRandomGenerator::system().
     *
     * \pre settings прошли проверку в PasswordModel::validate().
     */
    static QStringList generate(const PasswordSettings &settings);

private:
    //! Объединённый алфавит по выбранным наборам.
    static QString combinedAlphabet(const PasswordSettings &settings);

    //! Список алфавитов выбранных наборов (по одному на набор).
    static QStringList enabledAlphabets(const PasswordSettings &settings);

    //! Один пароль.
    static QString generateOne(const PasswordSettings &settings);

    //! Случайный символ из алфавита.
    static QChar randomChar(const QString &alphabet);
};

#endif // PASSWORDGENERATOR_H
