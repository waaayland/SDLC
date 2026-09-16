#include "model/passwordmodel.h"

#include "model/passwordgenerator.h"

#include <algorithm>

void PasswordModel::attach(IModelObserver *observer)
{
    if (!observer)
        return;
    if (std::find(m_observers.begin(), m_observers.end(), observer) == m_observers.end())
        m_observers.push_back(observer);
}

void PasswordModel::detach(IModelObserver *observer)
{
    m_observers.erase(std::remove(m_observers.begin(), m_observers.end(), observer),
                      m_observers.end());
}

void PasswordModel::notifySettingsChanged()
{
    for (IModelObserver *observer : m_observers)
        observer->onSettingsChanged(m_settings);
}

void PasswordModel::notifyPasswordsChanged()
{
    for (IModelObserver *observer : m_observers)
        observer->onPasswordsChanged(m_passwords);
}

void PasswordModel::notifyInputRejected(const QString &message)
{
    for (IModelObserver *observer : m_observers)
        observer->onInputRejected(message);
}

bool PasswordModel::validate(const RawPasswordInput &input,
                             PasswordSettings &settings,
                             QString &error) const
{
    bool ok = false;

    const QString lengthText = input.lengthText.trimmed();
    if (lengthText.isEmpty()) {
        error = QStringLiteral("Не указана длина пароля.");
        return false;
    }

    const int length = lengthText.toInt(&ok);
    if (!ok) {
        error = QStringLiteral("Длина пароля «%1» не является целым числом.")
                    .arg(lengthText);
        return false;
    }
    if (length < MinLength || length > MaxLength) {
        error = QStringLiteral("Длина пароля должна быть в диапазоне от %1 до %2 символов "
                               "(введено %3).")
                    .arg(MinLength).arg(MaxLength).arg(length);
        return false;
    }

    const QString countText = input.countText.trimmed();
    if (countText.isEmpty()) {
        error = QStringLiteral("Не указано количество паролей.");
        return false;
    }

    const int count = countText.toInt(&ok);
    if (!ok) {
        error = QStringLiteral("Количество паролей «%1» не является целым числом.")
                    .arg(countText);
        return false;
    }
    if (count < MinCount || count > MaxCount) {
        error = QStringLiteral("Количество паролей должно быть в диапазоне от %1 до %2 "
                               "(введено %3).")
                    .arg(MinCount).arg(MaxCount).arg(count);
        return false;
    }

    PasswordSettings candidate;
    candidate.length       = length;
    candidate.count        = count;
    candidate.useLowercase = input.useLowercase;
    candidate.useUppercase = input.useUppercase;
    candidate.useDigits    = input.useDigits;
    candidate.useSpecial   = input.useSpecial;

    const int setCount = candidate.enabledSetCount();
    if (setCount == 0) {
        error = QStringLiteral("Не выбран ни один набор символов. "
                               "Отметьте хотя бы один набор.");
        return false;
    }
    if (length < setCount) {
        error = QStringLiteral("Длина пароля (%1) меньше числа выбранных наборов символов (%2). "
                               "Каждый набор должен быть представлен хотя бы одним символом.")
                    .arg(length).arg(setCount);
        return false;
    }

    settings = candidate;
    return true;
}

void PasswordModel::submit(const RawPasswordInput &input)
{
    // Данные запоминаем всегда — даже некорректные, чтобы при повторном
    // открытии окна ввода пользователь увидел ровно то, что вводил.
    m_lastInput = input;

    PasswordSettings settings;
    QString error;

    if (!validate(input, settings, error)) {
        // Модель сама сообщает представлениям об ошибке.
        notifyInputRejected(error);
        return;
    }

    m_settings  = settings;
    m_passwords = PasswordGenerator::generate(settings);
    m_hasResult = true;

    // Модель сама рассылает уведомления об изменении состояния.
    notifySettingsChanged();
    notifyPasswordsChanged();
}
