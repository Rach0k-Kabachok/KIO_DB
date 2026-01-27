#pragma once

#include <vector>
#include <string>
#include <variant>
#include <fstream>
#include <stdexcept>
#include <iostream>

#include "schema.h"

class KioDbWriter {
public:
    using Column = std::variant<std::vector<int64_t>, std::vector<std::string>>;
    using ColumnarBatch = std::vector<Column>;

private:
    struct ColumnChunkMeta {
        uint8_t type;    // Schema::Types (INT64 = 0, STRING = 1)
        uint64_t offset; // Где начинается в файле
        uint64_t size;   // Размер в байтах
        uint64_t count;  // Количество элементов
    };

    struct BatchMeta {
        uint64_t batch_id;
        std::vector<ColumnChunkMeta> columns_info;
    };

    std::string db_filename_;
    std::ofstream db_file_;
    std::vector<BatchMeta> meta_index_;
    bool is_finished_ = false;

public:
    explicit KioDbWriter(const std::string& output_db_name)
        : db_filename_(output_db_name) {

        db_file_.open(db_filename_, std::ios::binary | std::ios::out);
        if (!db_file_.is_open()) {
            throw std::runtime_error("Failed to open output DB file: " + output_db_name);
        }
    }

    // Запрет копирования (владеем файловым дескриптором)
    KioDbWriter(const KioDbWriter&) = delete;
    KioDbWriter& operator=(const KioDbWriter&) = delete;

    // Разрешаем move
    KioDbWriter(KioDbWriter&&) = default;
    KioDbWriter& operator=(KioDbWriter&&) = default;

    ~KioDbWriter() {
        if (!is_finished_ && db_file_.is_open()) {
            try {
                Finish();
            } catch (const std::exception& e) {
                std::cerr << "Error in KioDbWriter destructor: " << e.what() << std::endl;
            }
        }
    }

    // Записывает батч на диск в колоночном формате
    void WriteBatch(const ColumnarBatch& batch, const Schema& schema) {
        if (batch.empty()) {
            return;
        }

        BatchMeta current_batch_meta;
        current_batch_meta.batch_id = meta_index_.size();

        // Проходим по всем колонкам батча
        for (size_t col_idx = 0; col_idx < batch.size(); ++col_idx) {
            const auto& column_variant = batch[col_idx];

            ColumnChunkMeta col_meta;

            // 1. Определяем тип из Schema
            Schema::Types col_type = schema.SearchTypeByIndex(col_idx);
            col_meta.type = static_cast<uint8_t>(col_type);

            // 2. Запоминаем позицию начала данных
            col_meta.offset = db_file_.tellp();

            // 3. Записываем данные в файл
            std::visit([this, &col_meta](const auto& vec) {
                WriteVectorToStream(vec, col_meta);
            }, column_variant);

            current_batch_meta.columns_info.push_back(col_meta);
        }

        meta_index_.push_back(current_batch_meta);
    }

    // Финализация: пишем Footer с метаданными
    void Finish() {
        if (is_finished_) {
            return;
        }

        // 1. Запоминаем позицию начала Footer
        uint64_t footer_start_pos = db_file_.tellp();

        // 2. Пишем количество батчей
        uint64_t batch_count = meta_index_.size();
        db_file_.write(reinterpret_cast<const char*>(&batch_count), sizeof(batch_count));

        // 3. Пишем метаданные каждого батча
        for (const auto& batch : meta_index_) {
            db_file_.write(reinterpret_cast<const char*>(&batch.batch_id), sizeof(batch.batch_id));

            // Пишем количество колонок в батче
            uint64_t col_count = batch.columns_info.size();
            db_file_.write(reinterpret_cast<const char*>(&col_count), sizeof(col_count));

            // Пишем метаданные каждой колонки (включая ТИП)
            for (const auto& col : batch.columns_info) {
                db_file_.write(reinterpret_cast<const char*>(&col.type), sizeof(col.type));
                db_file_.write(reinterpret_cast<const char*>(&col.offset), sizeof(col.offset));
                db_file_.write(reinterpret_cast<const char*>(&col.size), sizeof(col.size));
                db_file_.write(reinterpret_cast<const char*>(&col.count), sizeof(col.count));
            }
        }

        // 4. Пишем позицию Footer в конец файла (для быстрого поиска при чтении)
        db_file_.write(reinterpret_cast<const char*>(&footer_start_pos), sizeof(footer_start_pos));

        // 5. Закрываем файл
        db_file_.close();
        is_finished_ = true;
    }

private:
    // Запись вектора int64_t (фиксированный размер элемента)
    void WriteVectorToStream(const std::vector<int64_t>& vec, ColumnChunkMeta& meta) {
        meta.count = vec.size();
        uint64_t bytes = vec.size() * sizeof(int64_t);

        if (bytes > 0) {
            db_file_.write(reinterpret_cast<const char*>(vec.data()), bytes);
        }
        meta.size = bytes;
    }

    // Запись вектора string (переменный размер элемента)
    // Формат: [len: uint32][data: char*] [len: uint32][data: char*] ...
    void WriteVectorToStream(const std::vector<std::string>& vec, ColumnChunkMeta& meta) {
        meta.count = vec.size();
        uint64_t start = db_file_.tellp();

        for (const auto& s : vec) {
            uint32_t len = static_cast<uint32_t>(s.size());
            db_file_.write(reinterpret_cast<const char*>(&len), sizeof(len));
            if (len > 0) {
                db_file_.write(s.data(), len);
            }
        }

        uint64_t end = db_file_.tellp();
        meta.size = end - start;
    }
};
