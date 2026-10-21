# Тесты Arduino Uno + TMC2209 + HANPOSE 17HS8401

Основное подключение: STEP D2, DIR D3, EN D4; VIO 5 В, внешний БП на VM,
общий GND. Тесты рассчитаны на отдельные прошивки: выбирайте одно окружение
PlatformIO и загружайте его. Автоматическая загрузка не запускается этими
командами сборки. COM5 — порт проверенной сборки; на другом ПК укажите свой.

| Окружение PlatformIO | Назначение | Инструкция |
|---|---|---|
| `tmc2209_connection_test` | Медленные 400 импульсов вперёд/назад, автоматический запуск после 3 секунд | `src/tmc2209_connection_test.cpp` |
| `tmc2209_calibration_test` | Ровно 3200 импульсов по кнопке; определить число импульсов на оборот | Окно скорости с параметром `-Calibration` |
| `tmc2209_speed_test` | Плавный разгон, расчётные RPM, ручная остановка | [Тест скорости](tmc2209-speed-test-ru.md) |
| `tmc2209_fret_test` | Три переезда туда-обратно, расстояние 1–300 мм и время из окна | [Переезды между ладами](tmc2209-fret-test-ru.md) |
| `tmc2209_accuracy_test` | Точность хода и возврата: остановки для измерения, CSV и итоговый допуск | [Тест точности](tmc2209-accuracy-test-ru.md) |
| `tmc2209_uart_minimal` | Только чтение идентификатора UART через SoftwareSerial, без движения | [Минимальный UART](tmc2209-uart-minimal-ru.md) |
| `tmc2209_uart_minimal_alt` | Та же проверка через AltSoftSerial | [Минимальный UART](tmc2209-uart-minimal-ru.md) |
| `tmc2209_stallguard_test` | SG V3: сравнение свободного и нагруженного прохода, DIAG, проверка PWM | [StallGuard4](tmc2209-stallguard-test-ru.md) |

В `src/main.cpp` и `src/max_speed_test.cpp` сохранены прежние варианты
программы/теста с другой распиновкой: DIR D8, EN D13. Они не предназначены
для текущего подключения D2/D3/D4; новые тесты их не используют.

## Установка и загрузка

Нужен PlatformIO Core или расширение PlatformIO для VS Code. Для окон
управления нужен обычный Python с tkinter и pyserial. Библиотеки Arduino
указаны в `platformio.ini` и устанавливаются PlatformIO автоматически.
Python окружения PlatformIO может не включать tkinter: используйте обычный
Python для окна, а PlatformIO Python — для CLI/захвата порта.

Из корня репозитория:

```powershell
python -m pip install -r tools/requirements.txt
pio run -e tmc2209_fret_test
pio run -e tmc2209_fret_test -t upload --upload-port COM5
powershell -ExecutionPolicy Bypass -File .\tools\start_fret_control.ps1
```

Если команда `pio` отсутствует в PATH, замените её на:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e tmc2209_fret_test -t upload --upload-port COM5
```

Другие окна:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\start_speed_control.ps1
powershell -ExecutionPolicy Bypass -File .\tools\start_speed_control.ps1 -Calibration
powershell -ExecutionPolicy Bypass -File .\tools\start_stallguard_control.ps1
powershell -ExecutionPolicy Bypass -File .\tools\start_accuracy_control.ps1
```

Или запустите соответствующий `tools/tmc2209_*_control.py` обычным Python.
Закрывайте все окна, использующие COM-порт, перед загрузкой другой прошивки.

## Калибровка и ограничения

Проверено на конкретной сборке: 3200 импульсов дали 2 оборота, то есть
1600 импульсов/оборот. Это соответствует 1/8 для мотора 1,8°. Выберите 8
в окне скорости; поле только пересчитывает RPM, драйвер оно не настраивает.
Для переездов принят GT2 20 зубьев: 40 мм/оборот и 40 STEP/мм. При другой
механике расчёт расстояния нужно изменить.

StallGuard требует дополнительных UART и DIAG проводов: D8 RX, D9 TX через
1 кОм к тому же PDN_UART, DIAG D5, 10 кОм D5→GND. Схема и ограничения
подробно описаны в отдельной инструкции. Измерения SG зависят от тока,
скорости и питания; успешная остановка при пороге 255 не подтверждает
обнаружение реального препятствия. V3 сначала записывает свободный проход
без остановки по SG, затем проверяет нагруженный проход при тех же настройках.
При насыщении PWM измерение отклоняется; пределы хода и времени остаются.

Нет энкодера или поиска нуля: выдача импульсов не доказывает физическое
перемещение. Нет обработки концевиков. После срыва/экстренного останова
восстановите исходную точку вручную. Не переключайте моторные провода
под питанием. Сначала тестируйте свободный вал и обеспечьте свободный ход.

## Проверки разработчика

Сборка всех окружений без загрузки: `pio run`.
CRC, разбор эха и отбрасывание повреждённых UART-ответов проверяются
командой `python tools/check_stallguard_uart.py` (нужен host-компилятор g++).
`tools/read_uart_minimal.py` читает COM5 в течение 12 секунд и сохраняет
журнал; `--query` дополнительно отправляет `?`, без команд движения.

Проверены сборки и окна, на подключённом устройстве — чтение UART и
подтверждение настроек. Механические пределы и чувствительность StallGuard
нужно проверять на конкретной сборке под нагрузкой. Артефакты, логи и
кэш находятся в `.pio/` и `__pycache__/` и в Git не включаются.
