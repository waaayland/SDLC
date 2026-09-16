#include <QApplication>

#include "controller/passwordcontroller.h"
#include "model/passwordmodel.h"
#include "view/mainwindow.h"

/*!
 * Точка сборки приложения (composition root): здесь и только здесь
 * создаются и связываются три слоя MVC.
 *
 *   Model      — PasswordModel      (активная: сама оповещает наблюдателей)
 *   View       — MainWindow, InputDialog
 *   Controller — PasswordController
 */
int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Генератор паролей"));
    QApplication::setApplicationVersion(QStringLiteral("1.0.0"));
    QApplication::setOrganizationName(QStringLiteral("БГУИР"));

    PasswordModel      model;
    PasswordController controller(model);
    MainWindow         window(model, controller);

    // Контроллеру нужно окно-владелец, чтобы показывать модальный диалог ввода.
    controller.setView(&window);

    window.show();

    return app.exec();
}
