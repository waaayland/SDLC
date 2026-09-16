#include "view/inputdialog.h"
#include "ui_inputdialog.h"

InputDialog::InputDialog(const RawPasswordInput &initial, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::InputDialog)
{
    ui->setupUi(this);
    applyInitialValues(initial);
    ui->lineEditLength->setFocus();
    ui->lineEditLength->selectAll();
}

InputDialog::~InputDialog()
{
    delete ui;
}

void InputDialog::applyInitialValues(const RawPasswordInput &initial)
{
    ui->lineEditLength->setText(initial.lengthText);
    ui->lineEditCount->setText(initial.countText);
    ui->checkBoxLowercase->setChecked(initial.useLowercase);
    ui->checkBoxUppercase->setChecked(initial.useUppercase);
    ui->checkBoxDigits->setChecked(initial.useDigits);
    ui->checkBoxSpecial->setChecked(initial.useSpecial);
}

RawPasswordInput InputDialog::rawInput() const
{
    RawPasswordInput input;
    input.lengthText   = ui->lineEditLength->text();
    input.countText    = ui->lineEditCount->text();
    input.useLowercase = ui->checkBoxLowercase->isChecked();
    input.useUppercase = ui->checkBoxUppercase->isChecked();
    input.useDigits    = ui->checkBoxDigits->isChecked();
    input.useSpecial   = ui->checkBoxSpecial->isChecked();
    return input;
}
