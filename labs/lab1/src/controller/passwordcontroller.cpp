#include "controller/passwordcontroller.h"

#include "model/passwordmodel.h"
#include "view/inputdialog.h"

#include <QDialog>

PasswordController::PasswordController(PasswordModel &model, QObject *parent)
    : QObject(parent)
    , m_model(model)
{
}

void PasswordController::setView(QWidget *view)
{
    m_view = view;
}

void PasswordController::requestInput()
{
    // Дополнительное окно инициализируется последними введёнными данными,
    // которые хранит модель, — это и есть «восстановление при повторном вводе».
    InputDialog dialog(m_model.lastInput(), m_view);

    if (dialog.exec() != QDialog::Accepted)
        return;

    // Контроллер только передаёт данные модели. Дальше модель проверит их
    // и сама оповестит представления — обновлением экрана контроллер
    // не занимается.
    m_model.submit(dialog.rawInput());
}
