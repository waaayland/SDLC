#ifndef INPUTDIALOG_H
#define INPUTDIALOG_H

#include <QDialog>

#include "model/rawpasswordinput.h"

QT_BEGIN_NAMESPACE
namespace Ui { class InputDialog; }
QT_END_NAMESPACE

/*!
 * \brief Дополнительное окно ввода данных (часть слоя View).
 *
 * Открывается по кнопке «Ввести данные» главной формы. Окно только собирает
 * ввод и отдаёт его контроллеру: никакой проверки корректности и никакой
 * генерации здесь нет — это задача модели.
 *
 * Поля длины и количества сделаны текстовыми (QLineEdit), а не счётчиками,
 * чтобы пользователь мог ввести некорректные данные и увидеть сообщение
 * об ошибке, как требует задание.
 */
class InputDialog : public QDialog
{
    Q_OBJECT

public:
    /*!
     * \param initial последние введённые данные из модели — ими
     *        заполняются поля при повторном открытии окна.
     */
    explicit InputDialog(const RawPasswordInput &initial, QWidget *parent = nullptr);
    ~InputDialog() override;

    //! Данные, введённые пользователем, без какой-либо обработки.
    RawPasswordInput rawInput() const;

private:
    void applyInitialValues(const RawPasswordInput &initial);

    Ui::InputDialog *ui;
};

#endif // INPUTDIALOG_H
