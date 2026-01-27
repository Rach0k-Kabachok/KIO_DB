#pragma once

#include <string>

#include "csv_foramtter.h"
#include "schema.h"
#include "kio_db_writer.h"

class KioDb {
public:
    using ColumnarBatch = KioDbWriter::ColumnarBatch;

private:
    Schema schema_;
    CSVFormatter csv_formatter_;
    KioDbWriter writer_;

public:
    KioDb(const std::string& csv_filename, const std::string& schema_filename, const std::string& output_db_name)
        : csv_formatter_(csv_filename, schema_filename), writer_(output_db_name) {
    }

    // Запрет копирования
    KioDb(const KioDb&) = delete;
    KioDb& operator=(const KioDb&) = delete;

    // Разрешаем move
    KioDb(KioDb&&) = default;
    KioDb& operator=(KioDb&&) = default;

    ~KioDb() = default;

    // Читает все данные из CSV и пишет их в .kiodb файл
    void ProcessAllInput() {
        // Читаем и обрабатываем батчи
        while (ReadAndWriteNextBatch()) {
            // Каждый батч уже записан в файл
        }

        // Финализируем (пишем Footer)
        writer_.Finish();
    }

private:
    // Читает один батч из CSV и записывает его в файл
    bool ReadAndWriteNextBatch() {
        // Читаем схему при первом вызове
        if (schema_.IsEmpty()) {
            schema_ = csv_formatter_.GetSchema();
        }

        // Читаем батч из CSV
        ColumnarBatch batch = csv_formatter_.MakeColumnarBatch();

        if (batch.empty()) {
            return false; // Данные закончились
        }

        // Записываем батч на диск
        writer_.WriteBatch(batch, schema_);

        return true;
    }
};
