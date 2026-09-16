#ifndef PASSWORDMODEL_H
#define PASSWORDMODEL_H

#include <QString>
#include <QStringList>

#include <vector>

#include "model/imodelobserver.h"
#include "model/passwordsettings.h"
#include "model/rawpasswordinput.h"

/*!
 * \brief Модель приложения «Генератор паролей» (слой Model паттерна MVC).
 *
 * Модель АКТИВНАЯ:
 *  - хранит состояние (последний ввод, принятые параметры, готовые пароли);
 *  - сама проверяет корректность данных и выполняет генерацию;
 *  - сама рассылает уведомления всем подписанным представлениям
 *    (attach()/detach(), интерфейс IModelObserver).
 *
 * Модель не зависит от Qt Widgets и ничего не знает ни о представлениях,
 * ни о контроллере — общение идёт только через интерфейс наблюдателя.
 */
class PasswordModel
{
public:
    // Границы допустимых значений — бизнес-правила, поэтому живут в модели.
    static constexpr int MinLength = 4;
    static constexpr int MaxLength = 128;
    static constexpr int MinCount  = 1;
    static constexpr int MaxCount  = 50;

    PasswordModel() = default;

    // --- подписка представлений (Observer) ---
    void attach(IModelObserver *observer);
    void detach(IModelObserver *observer);

    // --- доступ к состоянию (для первичной отрисовки представления) ---
    const RawPasswordInput &lastInput() const { return m_lastInput; }
    const PasswordSettings &settings()  const { return m_settings; }
    const QStringList      &passwords() const { return m_passwords; }
    bool hasResult() const { return m_hasResult; }

    /*!
     * \brief Принять «сырые» данные из окна ввода.
     *
     * Единственная точка входа для контроллера. Модель запоминает ввод,
     * проверяет его, при успехе генерирует пароли и в любом случае сама
     * оповещает наблюдателей о результате.
     */
    void submit(const RawPasswordInput &input);

private:
    bool validate(const RawPasswordInput &input,
                  PasswordSettings &settings,
                  QString &error) const;

    // Рассылка уведомлений — выполняется самой моделью (активная модель).
    void notifySettingsChanged();
    void notifyPasswordsChanged();
    void notifyInputRejected(const QString &message);

    std::vector<IModelObserver *> m_observers;

    RawPasswordInput m_lastInput;   //!< последние введённые данные
    PasswordSettings m_settings;    //!< последние принятые параметры
    QStringList      m_passwords;   //!< последний результат генерации
    bool             m_hasResult = false;
};

#endif // PASSWORDMODEL_H
