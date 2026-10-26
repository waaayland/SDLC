package UAVLogContext {
    doc /* Системный контекст ПО «UAVLog — Журнал эксплуатации парка БЛА» */

    part def 'Программное средство';
    part def 'Пользователь';
    part def 'Хранилище';
    part def 'Внешнее устройство';

    part <context> 'Контекст UAVLog' {
        part <chief> 'Начальник службы эксплуатации' : 'Пользователь';
        part <tech> 'Техник' : 'Пользователь';
        part <oper> 'Оператор БЛА' : 'Пользователь';
        part <head> 'Руководитель подразделения' : 'Пользователь';

        part <sys> 'ПО UAVLog (АРМ, Windows)' : 'Программное средство';

        part <db> 'Файл базы данных SQLite' : 'Хранилище';
        part <bak> 'Резервные копии БД' : 'Хранилище';
        part <exp> 'Файлы отчётов CSV / PDF' : 'Хранилище';
        part <prn> 'Принтер' : 'Внешнее устройство';

        connection 'справочники, пользователи, отчёты' connect chief to sys;
        connection 'ТО, неисправности' connect tech to sys;
        connection 'записи о полётах' connect oper to sys;
        connection 'просмотр отчётов' connect head to sys;
        connection 'чтение / запись' connect sys to db;
        connection 'резервирование' connect sys to bak;
        connection 'экспорт' connect sys to exp;
        connection 'печать' connect sys to prn;
    }
}
