# KIO_DB

KIO_DB - экспериментальный колоночный движок хранения и выполнения запросов на
C++23. Проект умеет импортировать CSV в собственный бинарный формат `.kiodb`,
читать его обратно, экспортировать в CSV и выполнять набор заранее описанных
планов запросов из ClickBench.

## Возможности

- Конвертация CSV + schema CSV в колоночный `.kiodb`.
- Экспорт `.kiodb` обратно в CSV.
- Выполнение ClickBench-запросов с id `1..43`.
- Колоночное чтение только нужных колонок.
- Row groups с метаданными по батчам и column chunks.
- Min/max статистика по column chunk для пропуска неподходящих row groups.
- Типизированные агрегаты: `COUNT`, `SUM`, `AVG`, `MIN`, `MAX`,
  `COUNT DISTINCT`.
- Группировка, сортировка и Top-K без материализации лишних строк сверх
  необходимого результата.

## Структура проекта

- `src/global` - общие типы: схема, колоночные контейнеры, операции над
  колонками, scalar value и distinct/hash helpers.
- `src/transport/csv` - чтение CSV батчами, парсинг строк и полей, экспорт CSV.
- `src/transport/kio` - чтение, запись и импорт собственного формата `.kiodb`.
- `src/transport/compression` - кодирования колонок и базовая инфраструктура
  compression/encoding.
- `src/execution/operators` - физические операторы исполнения запросов.
- `src/execution/query_executor` - сборка планов ClickBench-запросов и helpers
  для построения операторного дерева.
- `tests` - unit/integration tests для CSV, KIO, compression и execution.
- `script` - shell-скрипты для сборки, конвертации, запуска запросов и
  Docker-бенчмарков.

## Формат `.kiodb`

Файл начинается с magic bytes `KIOD` и offset footer. Данные записываются
row groups: внутри каждой row group лежат column chunks, а footer хранит schema,
общее число строк и метаданные по всем row groups.

Для каждого column chunk сохраняются:

- локальный offset внутри row group;
- размер payload;
- выбранное column encoding;
- compression type;
- min/max значения, если их можно посчитать для типа колонки.

Сейчас compression layer оставляет данные без внешнего сжатия (`NONE`), но перед
записью выбирает типизированное кодирование колонки.

## Кодирования и оптимизации

- Для целочисленных типов, дат и timestamp используется delta encoding.
- Для `TEXT` и `VARCHAR` выбирается dictionary encoding.
- Для `CHAR` используется RLE.
- Для `DOUBLE` используется plain encoding.
- CSV читается батчами, чтобы не держать весь входной файл в строковом виде.
- `TableScanOperator` читает только запрошенные колонки и может пропускать row
  groups по min/max ограничениям.
- Global aggregates обрабатывают колонку пачкой, без отдельного вызова на
  каждую строку.
- Group by использует компактную hash table для закодированных ключей групп и
  отдельный accumulator для агрегатов.
- Ordered limit реализован как heap-based Top-K, когда полный sort не нужен.

## Сборка

Требования:

- CMake `3.22+`;
- компилятор с поддержкой C++23;
- bash для скриптов из `script`;
- доступ к GitHub при первой сборке тестов, потому что GoogleTest подключается
  через `FetchContent`.

Быстрая Release-сборка без тестов:

```bash
./script/build.sh
```

Ручная сборка:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

Сборка с тестами:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## Использование CLI

После сборки основной бинарь находится в `build/src/kio_db`.

Конвертировать CSV в `.kiodb`:

```bash
build/src/kio_db convert <input_csv> <schema_csv> <output_kiodb>
```

Экспортировать `.kiodb` в CSV:

```bash
build/src/kio_db export <input_kiodb> <output_csv>
```

Выполнить ClickBench-запрос:

```bash
build/src/kio_db query <input_kiodb> <query_id> <output_csv>
```

`query_id` - число от `1` до `43`.

Пример:

```bash
build/src/kio_db convert tests/hits.csv tests/hits_schema.csv /tmp/hits.kiodb
build/src/kio_db query /tmp/hits.kiodb 1 /tmp/query_1.csv
build/src/kio_db export /tmp/hits.kiodb /tmp/hits_export.csv
```

## Скрипты

- `script/build.sh` - Release-сборка без тестов.
- `script/convert.sh <input_csv> <output_kiodb>` - конвертация с записью времени
  выполнения в CSV.
- `script/run_query.sh <zero_based_query_id> <input_kiodb> <output_csv> <log>` -
  запуск запроса для benchmark pipeline. Скрипт принимает zero-based id и
  передает в CLI id на единицу больше.
- `script/make_docker.sh` и `script/run_docker.sh` - вспомогательные скрипты для
  Docker-сценария бенчмарков.

## Тесты

Тесты покрывают:

- парсинг CSV и round-trip CSV -> KIO -> CSV;
- column encodings;
- limit, sort, top-k, aggregate и ClickBench query plans.

Запуск:

```bash
ctest --test-dir build --output-on-failure
```
