#pragma once

#include <string>

#include "csv_work/csv_formatter.h"
#include "kio_work/kio_db_writer.h"
#include "columnar_types.h"
#include "schema.h"

class KioDb {
    Schema schema_;
    CSVFormatter csv_formatter_;
    KioDbWriter writer_;


    ctp::ColumnarBatch batch_;

public:
    KioDb(const std::string& csv_filename, const std::string& schema_filename,
          const std::string& output_db_name);

    KioDb(const KioDb&) = delete;
    KioDb& operator=(const KioDb&) = delete;

    KioDb(KioDb&&) = delete;
    KioDb& operator=(KioDb&&) = delete;

    // функция для удобного тестирования чтения-записи
    void ProcessAllInput();

    bool ReadNextBatch();

    void WriteBatchToKIO();
};
