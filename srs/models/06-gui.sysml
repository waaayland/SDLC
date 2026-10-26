package GuiLib {
    // Библиотека элементов графического интерфейса для мокапов
    private import ScalarValues::String;

    part def 'Окно';
    part def 'Диалог' :> 'Окно';
    part def 'Меню' { attribute 'пункты' : String[*]; }
    part def 'Панель' { attribute 'кнопки' : String[*]; }
    part def 'Группа полей';
    part def 'Поле ввода';
    part def 'Поле даты';
    part def 'Выпадающий список' { attribute 'значения' : String[*]; }
    part def 'Таблица' { attribute 'столбцы' : String[*]; }
    part def 'Вкладки';
    part def 'Индикатор' { attribute 'цвета' : String[*]; }
    part def 'Кнопка';
    part def 'Список';
    part def 'Статусная строка';

    // Связи между элементами окна (передаваемые данные)
    connection def 'Условие поиска' { end part 'откуда'; end part 'куда'; }
    connection def 'Выбранный борт' { end part 'откуда'; end part 'куда'; }
    connection def 'Итоги по парку' { end part 'откуда'; end part 'куда'; }
    connection def 'Результат проверки допуска' { end part 'откуда'; end part 'куда'; }
    connection def 'Длительность полёта' { end part 'откуда'; end part 'куда'; }
    connection def 'Перечень работ' { end part 'откуда'; end part 'куда'; }
    connection def 'Данные отчёта' { end part 'откуда'; end part 'куда'; }
}

package GuiMockups {
    private import GuiLib::*;
    // Нетипизированная связь connect задаёт порядок обхода полей клавишей Tab
    // (сверху вниз); типизированная связь показывает передачу данных.

    part <login> 'Окно «Вход в систему»' : 'Диалог' {
        part <user> 'Логин' : 'Поле ввода';
        part <pass> 'Пароль (символы скрыты)' : 'Поле ввода';
        part <err> 'Сообщение об ошибке входа' : 'Статусная строка';
        part <btns> 'Кнопки' : 'Панель' {
            attribute redefines 'кнопки' = ("Войти", "Выход");
        }
        connect user to pass;
        connect pass to btns;
        connection : 'Результат проверки допуска' connect btns to err;
    }

    part <main> 'Главное окно «Парк БЛА»' : 'Окно' {
        part <menu> 'Главное меню' : 'Меню' {
            attribute redefines 'пункты' = ("Файл", "Справочники", "Журналы", "Отчёты", "Сервис", "Справка");
        }
        part <tools> 'Панель инструментов' : 'Панель' {
            attribute redefines 'кнопки' = ("Добавить БЛА", "Зарегистрировать полёт", "Внести ТО", "Неисправность");
            part <search> 'Поле поиска' : 'Поле ввода';
        }
        part <fleet> 'Таблица парка БЛА' : 'Таблица' {
            attribute redefines 'столбцы' = ("Борт", "Тип", "Состояние", "Налёт, ч", "До ТО, ч / дн", "Последний полёт");
        }
        part <alerts> 'Панель предупреждений о ТО' : 'Список';
        part <status> 'Строка состояния' : 'Статусная строка';

        connect menu to tools;
        connection : 'Условие поиска' connect tools.search to fleet;
        connection : 'Выбранный борт' connect alerts to fleet;
        connection : 'Итоги по парку' connect fleet to status;
    }

    part <card> 'Окно «Формуляр БЛА»' : 'Окно' {
        part <head> 'Паспорт борта' : 'Группа полей' {
            part <f1> 'Бортовой номер' : 'Поле ввода';
            part <f2> 'Тип / модель' : 'Выпадающий список';
            part <f3> 'Заводской номер' : 'Поле ввода';
            part <f4> 'Дата ввода в эксплуатацию' : 'Поле даты';
            connect f1 to f2;
            connect f2 to f3;
            connect f3 to f4;
        }
        part <uavState> 'Состояние и наработка' : 'Индикатор' {
            attribute redefines 'цвета' = ("зелёный — допущен", "жёлтый — скоро ТО", "красный — не допущен");
        }
        part <tabs> 'Вкладки журналов' : 'Вкладки' {
            part 'Полёты' : 'Таблица';
            part 'ТО' : 'Таблица';
            part 'Неисправности' : 'Таблица';
            part 'АКБ и нагрузка' : 'Таблица';
        }
        part <btns> 'Кнопки' : 'Панель' {
            attribute redefines 'кнопки' = ("Редактировать", "Удалить", "Печать формуляра", "Закрыть");
        }
        connect head to tabs;
        connect uavState to tabs;
        connect tabs to btns;
    }

    part <flight> 'Диалог «Регистрация полёта»' : 'Диалог' {
        part <what> 'Что и кто' : 'Группа полей' {
            part <uav> 'Борт' : 'Выпадающий список';
            part <oper> 'Оператор' : 'Выпадающий список';
            part <bat> 'АКБ' : 'Выпадающий список';
            connect uav to oper;
            connect oper to bat;
        }
        part <whenWhere> 'Когда и где' : 'Группа полей' {
            part <d> 'Дата полёта' : 'Поле даты';
            part <t1> 'Время взлёта' : 'Поле ввода';
            part <t2> 'Время посадки' : 'Поле ввода';
            part <dur> 'Длительность (только чтение)' : 'Поле ввода';
            part <task> 'Задача и район полётов' : 'Поле ввода';
            connect d to t1;
            connect t1 to t2;
            connection : 'Длительность полёта' connect t2 to dur;
            connect dur to task;
        }
        part <check> 'Индикатор допуска' : 'Индикатор' {
            attribute redefines 'цвета' = ("зелёный — допущен", "красный — причина отказа");
        }
        part <btns> 'Кнопки' : 'Панель' {
            attribute redefines 'кнопки' = ("Сохранить", "Отмена");
        }
        connection : 'Результат проверки допуска' connect what to check;
        connect whenWhere to btns;
        connect check to btns;
    }

    part <maint> 'Диалог «Выполнение ТО»' : 'Диалог' {
        part <hdr> 'Реквизиты ТО' : 'Группа полей' {
            part <m1> 'Борт' : 'Выпадающий список';
            part <kind> 'Вид ТО по регламенту' : 'Выпадающий список';
            part <m3> 'Дата выполнения' : 'Поле даты';
            part <m4> 'Исполнитель' : 'Выпадающий список';
            part <m5> 'Наработка на момент ТО (авто)' : 'Поле ввода';
            connect m1 to kind;
            connect kind to m3;
            connect m3 to m4;
            connect m4 to m5;
        }
        part <works> 'Таблица работ' : 'Таблица' {
            attribute redefines 'столбцы' = ("Работа", "Выполнено", "Примечание");
        }
        part <btns> 'Кнопки' : 'Панель' {
            attribute redefines 'кнопки' = ("Сохранить", "Отмена");
        }
        connection : 'Перечень работ' connect hdr to works;
        connect works to btns;
    }

    part <reports> 'Окно «Отчёты»' : 'Окно' {
        part <params> 'Параметры отчёта' : 'Группа полей' {
            part <kind> 'Вид отчёта' : 'Выпадающий список' {
                attribute redefines 'значения' = ("Налёт по бортам", "Налёт по операторам", "Неисправности", "Выполнение ТО");
            }
            part <period> 'Период с / по' : 'Поле даты';
            part <flt> 'Фильтр: борт / оператор' : 'Выпадающий список';
            connect kind to period;
            connect period to flt;
        }
        part <preview> 'Предварительный просмотр' : 'Таблица';
        part <btns> 'Кнопки' : 'Панель' {
            attribute redefines 'кнопки' = ("Сформировать", "Экспорт CSV", "Печать");
        }
        connection : 'Данные отчёта' connect params to preview;
        connect preview to btns;
    }
}

package GuiNavigation {
    // Карта переходов между окнами (связь — переход по действию пользователя)

    part <nav> 'Карта окон UAVLog' {
        part <w0> 'Вход в систему';
        part <w1> 'Главное окно «Парк БЛА»';
        part <w2> 'Формуляр БЛА';
        part <w3> 'Регистрация полёта';
        part <w4> 'Выполнение ТО';
        part <w5> 'Регистрация неисправности';
        part <w6> 'Отчёты';
        part <w7> 'Справочники';

        connect w0 to w1;
        connect w1 to w2;
        connect w1 to w3;
        connect w1 to w4;
        connect w1 to w5;
        connect w1 to w6;
        connect w1 to w7;
        connect w2 to w4;
        connect w2 to w5;
    }
}
