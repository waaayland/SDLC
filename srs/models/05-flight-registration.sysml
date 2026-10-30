package FlightRegistration {
    doc /* Сценарий регистрации полёта с проверкой допуска */
    private import ScalarValues::Boolean;

    action def <RegisterFlight> 'Регистрация полёта' {
        // борт исправен, ТО не просрочено, ресурс АКБ не исчерпан
        attribute 'борт и АКБ допущены' : Boolean;
        // срок допуска оператора не истёк
        attribute 'оператор допущен' : Boolean;

        first start;
        then action <input> 'Ввести дату, борт, оператора, АКБ, время взлёта и посадки';
        then action <check> 'Проверить состояние борта, сроки ТО, ресурс АКБ и допуск оператора';
        then decide;
            if 'борт и АКБ допущены' and 'оператор допущен' then save;
            else reject;

        action <save> 'Сохранить запись и пересчитать налёт борта и циклы АКБ';
        then action <warn> 'Обновить предупреждения о ТО';
        then done;

        action <reject> 'Показать причину отказа, запись не сохранять';
        then done;
    }
}
