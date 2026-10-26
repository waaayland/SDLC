package UavStates {
    doc /* Жизненный цикл борта (состояние БЛА в реестре) */

    item def 'Порог ТО достигнут';
    item def 'ТО начато';
    item def 'ТО выполнено';
    item def 'Критичная неисправность';
    item def 'Неисправность устранена';

    state def <UavState> 'Состояние БЛА' {
        entry; then serviceable;

        state <serviceable> 'Исправен (допущен)';
        state <dueMaint> 'Требуется ТО';
        state <inMaint> 'На ТО';
        state <faulty> 'Неисправен';

        transition first serviceable accept : 'Порог ТО достигнут' then dueMaint;
        transition first serviceable accept : 'ТО начато' then inMaint;
        transition first dueMaint accept : 'ТО начато' then inMaint;
        transition first inMaint accept : 'ТО выполнено' then serviceable;
        transition first serviceable accept : 'Критичная неисправность' then faulty;
        transition first dueMaint accept : 'Критичная неисправность' then faulty;
        transition first faulty accept : 'Неисправность устранена' then serviceable;
    }
}
