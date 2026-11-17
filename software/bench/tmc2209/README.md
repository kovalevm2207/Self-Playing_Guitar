# Стенд Uno + TMC2209: прежние диагностические тесты

Самостоятельный проект PlatformIO и окна Python/tkinter для старого стенда
с HANPOSE 17HS8401. Это диагностические прошивки, реально выдающие STEP;
они загружаются отдельно от `firmware/song_receiver` и заменяют программу Uno.

Восстановлены из `codex/single-string-wiring` (`dbe0b60`), с правками FRET V3
из сохранённого stash (`2da1856`): исходник, окно, проверка последовательности
и инструкция. Добавлена сохранённая проверка окна `check_fret_control.py`.
Ветка и stash сохранены. Исходная логика тестов не менялась.
Платформа AVR закреплена на 5.1.0, default environment — ручной FRET-тест.
Старые варианты `uno` и `max_speed_test` с DIR D8 / EN D13 сюда не перенесены.

## Подключение для движения

Подключать по маркировке именно своего модуля TMC2209.

| Arduino Uno / источник | TMC2209 |
|---|---|
| D2 | STEP |
| D3 | DIR |
| D4 | EN / ENN, LOW включает драйвер |
| 5V Uno | VIO / VCC_IO — питание логики |
| GND Uno и минус БП | Общая GND драйвера |
| Плюс внешнего БП мотора | VM / VMOT |
| GND | MS1 и MS2: 1/8 в standalone, адрес UART 0 |
| Одна обмотка мотора | Выходы фазы A: OA1/OA2, часто A1/A2 |
| Вторая обмотка мотора | Выходы фазы B: OB1/OB2, часто B1/B2 |

Пары обмоток определяют омметром при снятом питании. Не ориентироваться
на цвета разных моторов. VIO и VM — разные цепи; напряжение БП мотора
не подавать на VIO или GPIO. Ограничение тока должно соответствовать мотору
и конкретному модулю. Перекоммутация двигателя выполняется без питания.
Питание двигателя приходит от отдельного БП, Uno — от USB.

Режим MS1/MS2, уровни VCC_IO и назначение ENN сверены с
[даташитом TMC2209](https://www.analog.com/media/en/technical-documentation/data-sheets/TMC2209_datasheet_rev1.09.pdf).
Эта таблица задаёт сигналы, а не ориентацию модуля или номера ножек его разъёма.

UART для FRET/скорости/калибровки не требуется. Для UART и StallGuard:
D8 RX → активный PDN_UART напрямую; D9 TX → 1 кОм → тот же PDN_UART.
Для StallGuard дополнительно DIAG → D5, 10 кОм от D5 к GND.
Подробнее: [минимальный UART](docs/tmc2209-uart-minimal-ru.md).

## Первый ручной прогон

Закрыть SelfGuitar, мониторы порта и другие окна управления. Сейчас COM5
соответствует USB Serial; при изменении подключения проверить фактический порт.
Команды ниже выполняются из корня репозитория в PowerShell.

Только сборка, без подключения к COM:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -d .\software\bench\tmc2209 -e tmc2209_fret_test
```

Загрузка диагностической прошивки в Uno, затем запуск окна:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -d .\software\bench\tmc2209 -e tmc2209_fret_test -t upload --upload-port COM5
powershell -ExecutionPolicy Bypass -File .\software\bench\tmc2209\tools\start_fret_control.ps1
```

1. Сначала проверить направление на свободном валу. На каретке вручную
   обеспечить свободный ход в обе стороны: HOME и концевики не обрабатываются.
2. Выбрать COM5, нажать «Подключить», дождаться `FRET READY V3`.
3. **Заменить старые 125 мм в поле расстояния на 5 мм**, время оставить 800 мс.
4. Нажать «Пуск»: три раза туда-обратно, всего шесть переездов.
5. Проверить оба конца хода и возврат к метке. `timer_late` должен быть 0;
   это проверка генератора импульсов, а не измерение перемещения двигателя.

При GT2 20 зубьев, моторе 1,8° и микрошаге 1/8 получается 40 STEP/мм:
5 мм соответствуют 200 импульсам на переезд. При другой передаче пересчитать.
«Стоп», пробел и Esc в активном окне прекращают импульсы и отключают драйвер;
без торможения возможна инерция. Потеря heartbeat останавливает тест за 2 с.

Для свободного вала доступен [тест скорости](docs/tmc2209-speed-test-ru.md).
`tmc2209_connection_test` самостоятельно начинает движение после 3 секунд:
это отдельный тест свободного вала, не первый запуск каретки.
Все варианты перечислены в [каталоге тестов](docs/tmc2209-tests-ru.md).

## Проверки без платы

Из этого каталога; нужны Python с tkinter/pyserial и host g++:

```powershell
python tools/check_fret_control.py
python tools/check_fret_sequence.py
python tools/check_accuracy_test.py
python tools/check_stallguard_uart.py
python tools/check_stallguard_readiness.py
python tools/check_stallguard_ui.py
```

Окна запускаются обычным Windows Python; существующие wrappers также находят
pyserial в окружении PlatformIO. Для другого ПК: `python -m pip install -r tools/requirements.txt`.
Сборка из WSL/Nix без загрузки: из `software` выполнить
`nix develop --command pio run -d bench/tmc2209 -e tmc2209_fret_test`.

Сборки и проверки без COM не подтверждают работу механики. Результаты натурного
прогона фиксируются отдельно: ход, время, фактические конечные позиции,
`timer_late`, возврат и признаки срыва.
