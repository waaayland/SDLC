#ifndef IMODELOBSERVER_H
#define IMODELOBSERVER_H

#include <QString>
#include <QStringList>

#include "model/passwordsettings.h"

/*!
 * \brief Интерфейс наблюдателя модели (паттерн Observer).
 *
 * Ключевой элемент схемы «MVC с АКТИВНОЙ моделью»: модель сама оповещает
 * все подписанные представления об изменении своего состояния, не обращаясь
 * при этом к контроллеру и ничего не зная о конкретных классах представлений.
 * Контроллер после передачи данных в модель в обновлении экрана не участвует.
 */
class IModelObserver
{
public:
    virtual ~IModelObserver() = default;

    //! Изменились принятые моделью параметры генерации.
    virtual void onSettingsChanged(const PasswordSettings &settings) = 0;

    //! Модель сформировала новый набор паролей.
    virtual void onPasswordsChanged(const QStringList &passwords) = 0;

    //! Модель отклонила введённые данные как некорректные.
    virtual void onInputRejected(const QString &message) = 0;
};

#endif // IMODELOBSERVER_H
