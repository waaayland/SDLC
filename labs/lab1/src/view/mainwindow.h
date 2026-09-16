#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include "model/imodelobserver.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class PasswordModel;
class PasswordController;

/*!
 * \brief Главная форма приложения (слой View паттерна MVC).
 *
 * Форма реализует IModelObserver и при создании подписывается на модель.
 * Всё, что она делает:
 *  - передаёт нажатие кнопки «Ввести данные» контроллеру;
 *  - отображает состояние, полученное от модели в методах onXxx().
 *
 * Никаких вычислений и проверок в представлении нет.
 */
class MainWindow : public QMainWindow, public IModelObserver
{
    Q_OBJECT

public:
    MainWindow(PasswordModel &model, PasswordController &controller, QWidget *parent = nullptr);
    ~MainWindow() override;

    // --- IModelObserver: вызывается самой моделью ---
    void onSettingsChanged(const PasswordSettings &settings) override;
    void onPasswordsChanged(const QStringList &passwords) override;
    void onInputRejected(const QString &message) override;

private:
    //! Первичная отрисовка по текущему состоянию модели.
    void showInitialState();

    Ui::MainWindow     *ui;
    PasswordModel      &m_model;
    PasswordController &m_controller;
};

#endif // MAINWINDOW_H
