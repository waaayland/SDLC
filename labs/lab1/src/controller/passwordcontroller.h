#ifndef PASSWORDCONTROLLER_H
#define PASSWORDCONTROLLER_H

#include <QObject>

class QWidget;
class PasswordModel;

/*!
 * \brief Контроллер приложения (слой Controller паттерна MVC).
 *
 * Единственная обязанность — обработка действий пользователя:
 * открыть дополнительное окно ввода, забрать из него данные и передать
 * их модели.
 *
 * Контроллер НЕ обновляет представление и не проверяет данные: при активной
 * модели проверка выполняется в модели, а обновление экрана инициирует сама
 * модель через IModelObserver.
 */
class PasswordController : public QObject
{
    Q_OBJECT

public:
    explicit PasswordController(PasswordModel &model, QObject *parent = nullptr);

    //! Окно-владелец для модальных диалогов (задаётся в main.cpp).
    void setView(QWidget *view);

public slots:
    //! Реакция на нажатие кнопки «Ввести данные» на главной форме.
    void requestInput();

private:
    PasswordModel &m_model;
    QWidget       *m_view = nullptr;
};

#endif // PASSWORDCONTROLLER_H
