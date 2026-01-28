#pragma once

#include <vector>
#include <string>
#include <variant>
#include <fstream>
#include <stdexcept>
#include <cstring>

namespace kio {
    using Column = std::variant<std::vector<int64_t>, std::vector<std::string>>;
    using ColumnarBatch = std::vector<Column>;

    struct ColumnChunkMeta {
        uint8_t type;    // Schema::Types (INT64 = 0, STRING = 1)
        uint64_t offset;
        uint64_t size;
        uint64_t count;
    };

    struct BatchMeta {
        uint64_t batch_id;
        std::vector<ColumnChunkMeta> columns_info;
    };
}

class KioDbReader {
    std::string filename_;
    std::ifstream file_;
    std::vector<kio::BatchMeta> meta_index_;

public:
    KioDbReader(const std::string& filename)
        : filename_(filename) {

        file_.open(filename_, std::ios::binary | std::ios::in);
        if (!file_.is_open()) {
            throw std::runtime_error("Cannot open DB file: " + filename_);
        }

        ReadFooter();
    }

    // Запрет копирования
    KioDbReader(const KioDbReader&) = delete;
    KioDbReader& operator=(const KioDbReader&) = delete;

    // Разрешаем move
    KioDbReader(KioDbReader&&) = default;
    KioDbReader& operator=(KioDbReader&&) = default;

    ~KioDbReader() {
        if (file_.is_open()) {
            file_.close();
        }
    }

    // Возвращает количество доступных батчей
    size_t GetBatchCount() const {
        return meta_index_.size();
    }

    // Читает конкретный батч по индексу
    kio::ColumnarBatch ReadBatch(size_t batch_index) {
        if (batch_index >= meta_index_.size()) {
            throw std::out_of_range("Batch index out of range: " + std::to_string(batch_index));
        }

        const auto& batch_meta = meta_index_[batch_index];
        kio::ColumnarBatch result_batch;
        result_batch.reserve(batch_meta.columns_info.size());

        // Читаем каждую колонку согласно её типу (из метаданных файла)
        for (const auto& col_meta : batch_meta.columns_info) {
            if (col_meta.type == 0) { // INT64
                result_batch.push_back(ReadIntColumn(col_meta));
            } else if (col_meta.type == 1) { // STRING
                result_batch.push_back(ReadStringColumn(col_meta));
            } else {
                throw std::runtime_error("Unknown column type in file: " + std::to_string(col_meta.type));
            }
        }

        return result_batch;
    }

private:
    // Парсинг метаданных из конца файла
    void ReadFooter() {
        // 1. Читаем смещение футера (последние 8 байт)
        file_.seekg(-static_cast<int>(sizeof(uint64_t)), std::ios::end);

        uint64_t footer_start_pos;
        file_.read(reinterpret_cast<char*>(&footer_start_pos), sizeof(footer_start_pos));

        if (!file_.good()) {
            throw std::runtime_error("Failed to read footer position from file");
        }

        // 2. Прыгаем к началу футера
        file_.seekg(footer_start_pos, std::ios::beg);

        // 3. Читаем количество батчей
        uint64_t batch_count;
        file_.read(reinterpret_cast<char*>(&batch_count), sizeof(batch_count));

        if (!file_.good()) {
            throw std::runtime_error("Failed to read batch count from footer");
        }

        // 4. Читаем индекс каждого батча
        for (size_t i = 0; i < batch_count; ++i) {
            kio::BatchMeta meta;

            // Читаем ID батча
            file_.read(reinterpret_cast<char*>(&meta.batch_id), sizeof(meta.batch_id));
            if (!file_.good()) {
                throw std::runtime_error("Failed to read batch ID at batch " + std::to_string(i));
            }

            // Читаем количество колонок в батче
            uint64_t col_count;
            file_.read(reinterpret_cast<char*>(&col_count), sizeof(col_count));
            if (!file_.good()) {
                throw std::runtime_error("Failed to read column count at batch " + std::to_string(i));
            }

            // Читаем метаданные каждой колонки (включая ТИП)
            for (size_t c = 0; c < col_count; ++c) {
                kio::ColumnChunkMeta col;

                file_.read(reinterpret_cast<char*>(&col.type), sizeof(col.type));
                file_.read(reinterpret_cast<char*>(&col.offset), sizeof(col.offset));
                file_.read(reinterpret_cast<char*>(&col.size), sizeof(col.size));
                file_.read(reinterpret_cast<char*>(&col.count), sizeof(col.count));

                if (!file_.good()) {
                    throw std::runtime_error("Failed to read column metadata at batch " +
                                           std::to_string(i) + " column " + std::to_string(c));
                }

                meta.columns_info.push_back(col);
            }
            meta_index_.push_back(meta);
        }
    }

    // Чтение int64 колонки
    std::vector<int64_t> ReadIntColumn(const kio::ColumnChunkMeta& meta) {
        std::vector<int64_t> vec(meta.count);

        if (meta.count == 0) {
            return vec;
        }

        file_.seekg(meta.offset, std::ios::beg);
        file_.read(reinterpret_cast<char*>(vec.data()), meta.size);

        if (!file_.good()) {
            throw std::runtime_error("Failed to read int column at offset " + std::to_string(meta.offset));
        }

        return vec;
    }

    // Чтение string колонки
    std::vector<std::string> ReadStringColumn(const kio::ColumnChunkMeta& meta) {
        std::vector<std::string> vec;
        vec.reserve(meta.count);

        if (meta.count == 0) {
            return vec;
        }

        file_.seekg(meta.offset, std::ios::beg);

        for (size_t i = 0; i < meta.count; ++i) {
            uint32_t len;
            file_.read(reinterpret_cast<char*>(&len), sizeof(len));

            if (!file_.good()) {
                throw std::runtime_error("Failed to read string length at index " + std::to_string(i));
            }

            std::string s;
            if (len > 0) {
                s.resize(len);
                file_.read(s.data(), len);

                if (!file_.good()) {
                    throw std::runtime_error("Failed to read string data at index " + std::to_string(i));
                }
            }
            vec.push_back(std::move(s));
        }

        return vec;
    }
};
