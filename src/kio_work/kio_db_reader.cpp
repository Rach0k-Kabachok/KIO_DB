#include "kio_work/kio_db_reader.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "columnar_types.h"
#include "schema.h"


KioDbReader::KioDbReader(const std::string &filename) : filename_(filename) {
    file_.open(filename_, std::ios::binary | std::ios::in);
    if (!file_.is_open()) {
        throw std::runtime_error("Cannot open DB file: " + filename_);
    }

    BuildIndexFromEnd();
}


KioDbReader::~KioDbReader() {
    if (file_.is_open()) {
        file_.close();
    }
}


size_t KioDbReader::GetBatchCount() const {
    return meta_index_.size();
}


ctp::ColumnarBatch KioDbReader::ReadBatch(size_t batch_index) {
    if (batch_index >= meta_index_.size()) {
        throw std::out_of_range("Batch index out of range: " +
                                std::to_string(batch_index));
    }

    const auto &batch_meta = meta_index_[batch_index];

    ctp::ColumnarBatch result;
    result.reserve(batch_meta.columns_info.size());

    for (const auto &col_meta: batch_meta.columns_info) {
        if (col_meta.type == Schema::INT64) {
            result.push_back(ReadIntColumn(col_meta));
        } else if (col_meta.type == Schema::STRING) {
            result.push_back(ReadStringColumn(col_meta));
        } else {
            throw std::runtime_error("Unknown column type in file: " +
                                     std::to_string(col_meta.type));
        }
    }

    return result;
}


void KioDbReader::BuildIndexFromEnd() {
    meta_index_.clear();

    file_.seekg(0, std::ios::end);
    std::uint64_t end_pos = static_cast<std::uint64_t>(file_.tellg());
    if (!file_.good()) {
        throw std::runtime_error("Failed to seek/tell end of file");
    }

    std::vector<kio::BatchMeta> reversed;

    while (end_pos > 0) {
        if (end_pos < kTrailerSize) {
            throw std::runtime_error(
                "File too small or corrupted: no room for trailer");
        }

        // Читаем magic в самом конце, чтобы убедиться, что это конец батча.
        std::uint32_t magic = 0;
        file_.seekg(static_cast<std::streamoff>(end_pos - sizeof(magic)),
                    std::ios::beg);
        file_.read(reinterpret_cast<char *>(&magic), sizeof(magic));
        if (!file_.good()) {
            throw std::runtime_error("Failed to read batch magic");
        }
        if (magic != kBatchMetaMagic) {
            throw std::runtime_error("Bad batch magic (corrupted format?)");
        }

        // Читаем meta_size прямо перед magic
        std::uint64_t meta_size = 0;
        file_.seekg(static_cast<std::streamoff>(end_pos - kTrailerSize),
                    std::ios::beg);
        file_.read(reinterpret_cast<char *>(&meta_size), sizeof(meta_size));
        if (!file_.good()) {
            throw std::runtime_error("Failed to read meta_size");
        }

        const std::uint64_t meta_start = end_pos - kTrailerSize - meta_size;
        if (meta_start > end_pos) {
            throw std::runtime_error(
                "Invalid meta_start computed (corrupted meta_size)");
        }

        // Читаем BatchMeta block
        file_.seekg(static_cast<std::streamoff>(meta_start), std::ios::beg);

        kio::BatchMeta meta{};
        file_.read(reinterpret_cast<char *>(&meta.batch_id),
                   sizeof(meta.batch_id));
        if (!file_.good()) {
            throw std::runtime_error("Failed to read batch_id");
        }

        std::uint64_t col_count = 0;
        file_.read(reinterpret_cast<char *>(&col_count), sizeof(col_count));
        if (!file_.good()) {
            throw std::runtime_error("Failed to read col_count");
        }

        meta.columns_info.clear();
        meta.columns_info.reserve(static_cast<size_t>(col_count));

        for (std::uint64_t c = 0; c < col_count; ++c) {
            kio::ColumnChunkMeta col{};
            file_.read(reinterpret_cast<char *>(&col.type),
                       sizeof(col.type));
            file_.read(reinterpret_cast<char *>(&col.offset),
                       sizeof(col.offset));
            file_.read(reinterpret_cast<char *>(&col.size),
                       sizeof(col.size));
            file_.read(reinterpret_cast<char *>(&col.count),
                       sizeof(col.count));
            if (!file_.good()) {
                throw std::runtime_error(
                    "Failed to read column meta at batch_id=" +
                    std::to_string(meta.batch_id));
            }
            meta.columns_info.push_back(col);
        }

        reversed.push_back(std::move(meta));

        //начало батча - min(offset) по колонкам.
        const auto &last_meta = reversed.back();
        if (last_meta.columns_info.empty()) {
            throw std::runtime_error(
                "Batch has zero columns (unsupported/corrupted)");
        }

        std::uint64_t batch_start = last_meta.columns_info[0].offset;
        for (const auto &col: last_meta.columns_info) {
            batch_start = std::min(batch_start, col.offset);
        }

        end_pos = batch_start;
    }

    std::reverse(reversed.begin(), reversed.end());
    meta_index_ = std::move(reversed);
}


std::vector<std::int64_t> KioDbReader::ReadIntColumn(const kio::ColumnChunkMeta &meta) {
    if (meta.count == 0) {
        return {};
    }

    const std::uint64_t expected_bytes = meta.count * sizeof(std::int64_t);
    if (meta.size != expected_bytes) {
        throw std::runtime_error("Int column size mismatch at offset " +
                                 std::to_string(meta.offset));
    }

    std::vector<std::int64_t> vec(static_cast<size_t>(meta.count));

    file_.seekg(static_cast<std::streamoff>(meta.offset), std::ios::beg);
    file_.read(reinterpret_cast<char *>(vec.data()),
               static_cast<std::streamsize>(expected_bytes));

    if (!file_.good()) {
        throw std::runtime_error("Failed to read int column at offset " +
                                 std::to_string(meta.offset));
    }

    return vec;
}


std::vector<std::string> KioDbReader::ReadStringColumn(
    const kio::ColumnChunkMeta &meta) {
    std::vector<std::string> vec;
    vec.reserve(static_cast<size_t>(meta.count));

    file_.seekg(static_cast<std::streamoff>(meta.offset), std::ios::beg);

    for (std::uint64_t i = 0; i < meta.count; ++i) {
        std::uint32_t len = 0;
        file_.read(reinterpret_cast<char *>(&len), sizeof(len));
        if (!file_.good()) {
            throw std::runtime_error(
                "Failed to read string length at index " +
                std::to_string(i));
        }

        std::string s;
        if (len > 0) {
            s.resize(len);
            file_.read(s.data(), len);
            if (!file_.good()) {
                throw std::runtime_error(
                    "Failed to read string data at index " +
                    std::to_string(i));
            }
        }
        vec.push_back(std::move(s));
    }

    return vec;
}
