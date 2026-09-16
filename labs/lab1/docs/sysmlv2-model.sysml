// Лабораторная работа №1, вариант 15 — «Генератор паролей»
// Модель системы в текстовой нотации SysML v2 (KerML/SysML v2 textual notation).
// Просмотр: расширение «SysML v2» / «syside» для VS Code,
//           плагин SysIDE / SysML v2 для IntelliJ IDEA и Eclipse.

package PasswordGeneratorApp {

    doc
    /*
     * Приложение «Генератор паролей», построенное по схеме MVC
     * с АКТИВНОЙ моделью: модель самостоятельно оповещает подписанные
     * представления об изменении своего состояния.
     */

    // ---------------------------------------------------------------
    // Типы данных
    // ---------------------------------------------------------------

    part def CharacterSet {
        attribute name : String;
        attribute alphabet : String;
    }

    attribute def RawPasswordInput {
        doc /* «Сырые» данные из окна ввода — до проверки. */
        attribute lengthText   : String;
        attribute countText    : String;
        attribute useLowercase : Boolean;
        attribute useUppercase : Boolean;
        attribute useDigits    : Boolean;
        attribute useSpecial   : Boolean;
    }

    attribute def PasswordSettings {
        doc /* Проверенные параметры генерации. */
        attribute length       : Integer;
        attribute count        : Integer;
        attribute useLowercase : Boolean;
        attribute useUppercase : Boolean;
        attribute useDigits    : Boolean;
        attribute useSpecial   : Boolean;
    }

    // ---------------------------------------------------------------
    // Структура системы (три слоя MVC)
    // ---------------------------------------------------------------

    part def Application {

        part model : Model;
        part view : View;
        part controller : Controller;

        // Контроллер передаёт данные модели
        connect controller.toModel to model.fromController;

        // Модель сама оповещает представление (активная модель)
        connect model.toObservers to view.fromModel;

        // Представление передаёт действия пользователя контроллеру
        connect view.toController to controller.fromView;
    }

    part def Model {
        doc /* Слой Model. Активная модель. */

        port fromController : ~SubmitPort;
        port toObservers    : NotifyPort;

        attribute lastInput : RawPasswordInput;
        attribute settings  : PasswordSettings;
        attribute passwords : String[0..50];
        attribute hasResult : Boolean default false;

        // Бизнес-правила (границы допустимых значений)
        attribute minLength : Integer default 4;
        attribute maxLength : Integer default 128;
        attribute minCount  : Integer default 1;
        attribute maxCount  : Integer default 50;

        part generator : PasswordGenerator;
    }

    part def PasswordGenerator {
        doc /* Алгоритм генерации, отделённый от состояния модели. */
        part sets : CharacterSet[1..4];
    }

    part def View {
        doc /* Слой View: главная форма и дополнительное окно ввода. */

        port fromModel    : ~NotifyPort;
        port toController : ActionPort;

        part mainWindow : MainWindow;
        part inputDialog : InputDialog[0..1];
    }

    part def MainWindow {
        attribute displayedLength : Integer;
        attribute displayedCount  : Integer;
        attribute displayedSets   : String;
        attribute displayedPasswords : String[0..50];
    }

    part def InputDialog {
        doc /* Модальное окно; поля заполняются последним вводом из модели. */
        attribute initial : RawPasswordInput;
        attribute entered : RawPasswordInput;
    }

    part def Controller {
        doc /* Слой Controller: только обработка действий пользователя. */
        port fromView : ~ActionPort;
        port toModel  : SubmitPort;
    }

    // ---------------------------------------------------------------
    // Интерфейсы (порты)
    // ---------------------------------------------------------------

    port def ActionPort {
        out item enterDataRequested : RawPasswordInput[0..1];
    }

    port def SubmitPort {
        out item submitted : RawPasswordInput;
    }

    port def NotifyPort {
        doc /* Уведомления, которые модель рассылает сама. */
        out item settingsChanged  : PasswordSettings[0..1];
        out item passwordsChanged : String[0..50];
        out item inputRejected    : String[0..1];
    }

    // ---------------------------------------------------------------
    // Поведение: сценарий генерации паролей
    // ---------------------------------------------------------------

    action def GeneratePasswords {

        in  rawInput : RawPasswordInput;
        out result   : String[0..50];

        action rememberInput {
            doc /* Модель запоминает ввод до проверки. */
        }

        action validateInput {
            doc /* Проверка: числа, диапазоны, наличие хотя бы одного набора. */
            out valid : Boolean;
            out error : String;
        }

        action generate {
            doc /* В каждом пароле есть символ из каждого выбранного набора. */
        }

        action notifyObservers {
            doc /* Модель сама рассылает уведомления представлениям. */
        }

        action reportError {
            doc /* Модель сама рассылает сообщение об ошибке. */
        }

        first start then rememberInput;
        then validateInput;

        succession validateInput then generate
            if validateInput.valid == true;
        succession validateInput then reportError
            if validateInput.valid == false;

        then generate then notifyObservers;
        then notifyObservers then done;
        succession reportError then done;
    }

    // ---------------------------------------------------------------
    // Требования по варианту 15
    // ---------------------------------------------------------------

    requirement def <'R1'> InputParameters {
        doc
        /*
         * Пользователь вводит длину пароля, набор(ы) символов,
         * включаемых в пароль, и количество паролей.
         */
    }

    requirement def <'R2'> SeparateInputWindow {
        doc
        /*
         * На главной форме — кнопка «Ввести данные». По нажатию
         * открывается отдельное (модальное) окно ввода.
         */
    }

    requirement def <'R3'> RememberAndRestore {
        doc
        /*
         * Последние введённые данные запоминаются и восстанавливаются
         * при повторном открытии окна ввода.
         */
    }

    requirement def <'R4'> ShowResultOnMainForm {
        doc
        /*
         * Результат (параметры и сгенерированные пароли) отображается
         * на главной форме.
         */
    }

    requirement def <'R5'> ErrorMessage {
        doc
        /*
         * При некорректных данных выводится сообщение об ошибке.
         */
    }

    requirement def <'R6'> MvcActiveModel {
        doc
        /*
         * Реализован каркас MVC с активной моделью: модель сама
         * оповещает представления об изменении состояния.
         */
    }

    part variant15 : Application {
        satisfy requirement InputParameters;
        satisfy requirement SeparateInputWindow;
        satisfy requirement RememberAndRestore;
        satisfy requirement ShowResultOnMainForm;
        satisfy requirement ErrorMessage;
        satisfy requirement MvcActiveModel;
    }
}
