#pragma once

#include <string>
#include <fstream>
#include <vector>
#include <variant>
#include <iostream>
#include <stdexcept>

#include "kio_db_reader.h"

class CsvExporter {
public:
    static void Export(KioDbReader& reader, const std::string& out_filename) {
        std::ofstream out(out_filename);
        if (!out.is_open()) {
            throw std::runtime_error("Cannot open output CSV file: " + out_filename);
        }

        size_t batch_count = reader.GetBatchCount();

        if (batch_count == 0) {
            std::cout << "No batches to export" << std::endl;
            return;
        }

        for (size_t i = 0; i < batch_count; ++i) {
            try {
                auto batch = reader.ReadBatch(i);
                if (batch.empty()) {
                    std::cerr << "Warning: Batch " << i << " is empty, skipping" << std::endl;
                    continue;
                }

                WriteBatchToStream(batch, out);
            } catch (const std::exception& e) {
                throw std::runtime_error("Error exporting batch " + std::to_string(i) + ": " + e.what());
            }
        }

        out.close();
        if (!out.good()) {
            throw std::runtime_error("Error writing to output CSV file");
        }

        std::cout << "Successfully exported " << batch_count << " batches to " << out_filename << std::endl;
    }

    // Экспорт одного батча (для удобства)
    static void ExportBatch(KioDbReader& reader, size_t batch_index, const std::string& out_filename) {
        std::ofstream out(out_filename);
        if (!out.is_open()) {
            throw std::runtime_error("Cannot open output CSV file: " + out_filename);
        }

        try {
            auto batch = reader.ReadBatch(batch_index);
            if (batch.empty()) {
                throw std::runtime_error("Batch is empty");
            }

            WriteBatchToStream(batch, out);
        } catch (const std::exception& e) {
            throw std::runtime_error("Error exporting batch " + std::to_string(batch_index) + ": " + e.what());
        }

        out.close();
        if (!out.good()) {
            throw std::runtime_error("Error writing to output CSV file");
        }

        std::cout << "Successfully exported batch " << batch_index << " to " << out_filename << std::endl;
    }

private:
    // Запись батча в поток (CSV формат)
    static void WriteBatchToStream(const Kio::ColumnarBatch& batch, std::ostream& out) {
        if (batch.empty()) {
            return;
        }

        // 1. Узнаем количество строк в батче (по размеру первой колонки)
        size_t num_rows = std::visit([](const auto& v) { return v.size(); }, batch[0]);
        size_t num_cols = batch.size();

        // Проверка: все ли колонки имеют одинаковый размер
        for (size_t col_idx = 0; col_idx < num_cols; ++col_idx) {
            size_t col_size = std::visit([](const auto& v) { return v.size(); }, batch[col_idx]);
            if (col_size != num_rows) {
                throw std::runtime_error("Column " + std::to_string(col_idx) +
                                       " has " + std::to_string(col_size) +
                                       " rows, expected " + std::to_string(num_rows));
            }
        }

        // 2. Пишем построчно
        for (size_t row_idx = 0; row_idx < num_rows; ++row_idx) {

            for (size_t col_idx = 0; col_idx < num_cols; ++col_idx) {
                const auto& column_variant = batch[col_idx];

                // Используем visit для записи значения (работает для int64 и string)
                std::visit([&out, row_idx](const auto& vec) {
                    out << vec[row_idx];
                }, column_variant);

                // Добавляем запятую, если это не последняя колонка
                if (col_idx < num_cols - 1) {
                    out << ",";
                }
            }

            // Конец строки
            out << "\n";
        }
    }
};
