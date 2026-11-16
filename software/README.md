# SelfGuitar Desktop

Прототип каталога и USB-загрузки нот на Arduino Uno. C++17, Qt Quick/QML,
Qt Serial Port. Музыкальная модель и сценарии отделены от Qt-адаптеров
и интерфейса. [Архитектура и границы компонентов](docs/architecture.md).

## Структура ПО

```text
software/
  core/          музыкальная модель и сценарии без Qt
  desktop/       адаптеры, модели представления, QML и ресурсы
  firmware/      программы для контроллера
  shared/        общий формат нот и протокол
  tests/         тесты протокола и приёмника
  tools/         запуск, упаковка и проверка стиля
  docs/          документация протокола и правила оформления
  CMakeLists.txt
  CMakePresets.json
  flake.nix
  flake.lock
```

Исходники, зависимости, инструкции и результаты сборки ПО находятся внутри
этого каталога. Документация устройства и лифлет находятся в `../docs/`.

## Окружение и сборка

Основной путь разработки — Linux/WSL, как в проекте triangles:
Nix предоставляет зависимости, CMake собирает приложение, Ninja выполняет сборку.
Корневые flake.nix и flake.lock закрепляют Nixpkgs и nixGL, LLVM 18 и Qt.
Загрузка библиотек в CMake и локальные SDK для этого пути не используются.

Из корня репозитория сначала перейдите в каталог ПО:

```bash
cd software
nix develop
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
bash tools/run-linux.sh
```

Конфигурации debug, release, profile и quality соответствуют triangles.
Результат: build/<preset>/bin/SelfGuitar. profile отключает тесты,
quality создаёт compile_commands.json для clang-tidy.

В обычном Linux можно запускать build/debug/bin/SelfGuitar напрямую из окружения;
для графических драйверов хоста доступен nixGLIntel.
В WSL скрипт tools/run-linux.sh сохраняет программный рендеринг Qt Quick и XWayland,
если пользователь не указал другой QT_QUICK_BACKEND или QT_QPA_PLATFORM.

Быстрый запуск вне Nix-оболочки, из корня репозитория:

```bash
bash software/tools/start.sh
# Пересобрать и запустить:
bash software/tools/start.sh --build
# Сборка и тесты без окна:
bash software/tools/start.sh --check
```

Скрипты только вызывают nix develop и CMake presets. Обычный запуск использует
готовый бинарник; после изменения исходников требуется --build.
Для другого preset: GUITAR_PRESET=release bash software/tools/start.sh --build.
Глобальный канал <nixpkgs> не нужен. При добавлении flake-файлов в новый checkout
они должны быть известны Git, чтобы Nix видел исходники.

Без Qt можно собрать проверки протокола (из `software/`):

```bash
cmake -S . -B build/protocol -G Ninja -DGUITAR_BUILD_APP=OFF
cmake --build build/protocol
ctest --test-dir build/protocol --output-on-failure
```

Проверки Nix: nix flake check. Они отдельно собирают протокол и приложение,
выполняют CTest и проверяют запуск интерфейса в offscreen-режиме.

## Оформление

.clang-format скопирован из triangles. Используется clang-format 18,
для QML — qmlformat из закреплённого Qt. [Правила оформления](docs/code-style.md).

```bash
cmake --build build/debug --target format
cmake --build build/debug --target format-check
```

## Возможности и ограничения

- Локальный каталог из шести коротких учебных аранжировок, поиск и избранное.
- Фильтр по количеству струн, подсветка используемых струн и выбор темпа.
- USB-handshake, подтверждение каждой ноты, CRC-16, таймаут и отмена.
- Каталог может хранить длинные аранжировки. Текущая прошивка принимает
  до 96 нот в RAM; отключение питания стирает песню.
- Прошивка пока не управляет механикой и не воспроизводит мелодию.

desktop/data/songs.json содержит два массива: songs с названиями/авторами
и arrangements с версиями исполнения. У каждой аранжировки есть собственный id
и ссылка songId. Старые ID аранжировок сохранены, поэтому избранное совместимо.
Избранное хранится атомарно в favorites.json каталога настроек приложения
(QStandardPaths::AppConfigLocation). До первого сохранения читаются прежние
настройки QSettings. Сбой избранного не блокирует просмотр и загрузку песен.
Ноты записываются в формате [струна, лад, длительность в мс].
Количество струн вычисляется по нотам. Струна 1 — самая тонкая E4, 6 — E2;
лады 0–24, длительность 50–10000 мс. bpm задаёт базовый темп аранжировки.
События последовательные, без аккордов и пауз. Это тестовые фрагменты,
а не каталог полных песен.

## Arduino

Реальная передача пока не проверена: плата не подключена.
При подключении сначала отдельно загрузите прошивку приёмника
(она заменит существующую программу платы), из `software/`:

```bash
pio run -d firmware/song_receiver -t upload --upload-port /dev/ttyACM0
```

Укажите фактический порт. В Windows это COM-порт. Для WSL потребуется передать
USB-устройство в Linux; доступ к COM-порту Windows автоматически не появляется.
Выберите порт в приложении, подключитесь и загрузите аранжировку.
[Описание протокола](docs/protocol.md).

## Windows

Для нативной сборки требуется
отдельный Qt SDK с MinGW; это дополнительный путь, не Nix-окружение Linux.
Скрипт tools/build-windows.ps1 использует CMakeLists.txt каталога software
и сохраняет сборку в software/build/windows. Флаг -Package собирает пакет
в software/build/windows/package. Запуск из корня репозитория:

```powershell
./software/tools/start.ps1
```

Готовые бинарники и DLL не хранятся в Git; старые сборки удаляются как кэш.

## Сохранённые материалы оборудования

Ветка software создана от main. История оборудования находится в
codex/single-string-wiring; прежние изменённые файлы сохранены в именованном stash.
Локальные CAD-скрипты и промежуточные материалы сохранены в archive/local-hardware.
Они, SDK и результаты сборки исключены из Git.
