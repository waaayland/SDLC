package FlightRegistration {
    doc /* Сценарий регистрации полёта с проверкой допуска борта */
    private import ScalarValues::Boolean;

    action def <RegisterFlight> 'Регистрация полёта' {
        // борт исправен и ТО не просрочено
        attribute 'борт допущен' : Boolean;

        first start;
        then action <input> 'Ввести дату, борт, оператора, АКБ, время взлёта и посадки';
        then action <check> 'Проверить состояние борта и сроки ТО';
        then decide;
            if 'борт допущен' then save;
            else reject;

        action <save> 'Сохранить запись и пересчитать налёт борта и циклы АКБ';
        then action <warn> 'Обновить предупреждения о ТО';
        then done;

        action <reject> 'Показать причину отказа, запись не сохранять';
        then done;
    }
}
