#include "kio_work/kio_db_writer.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <variant>

#include "columnar_types.h"
#include "schema.h"



KioDbWriter::KioDbWriter(const std::string &output_db_name)
    : db_filename_(output_db_name) {
    db_file_.open(db_filename_,
                  std::ios::binary | std::ios::out | std::ios::trunc);
    if (!db_file_.is_open()) {
        throw std::runtime_error("Failed to open output DB file: " +
                                 output_db_name);
    }
}


KioDbWriter::~KioDbWriter() {
    if (!is_finished_ && db_file_.is_open()) {
        try {
            Finish();
        } catch (const std::exception &e) {
            std::cerr << "Error in KioDbWriter destructor: " << e.what()
                    << std::endl;
        }
    }
}


void KioDbWriter::WriteBatch(const ctp::ColumnarBatch &batch, const Schema &schema) {
    if (batch.empty()) {
        return;
    }

    kio::BatchMeta meta;
    meta.batch_id = next_batch_id_++;
    meta.columns_info.reserve(batch.size());

    for (size_t col_idx = 0; col_idx < batch.size(); ++col_idx) {
        kio::ColumnChunkMeta col_meta{};
        col_meta.type =
                static_cast<std::uint8_t>(schema.SearchTypeByIndex(col_idx));

        const auto off = db_file_.tellp();
        if (off == std::ofstream::pos_type(-1)) {
            throw std::runtime_error(
                "tellp() failed before writing column");
        }
        col_meta.offset = static_cast<std::uint64_t>(off);

        const auto &column_variant = batch[col_idx];
        std::visit(
            [this, &col_meta](const auto &vec) {
                WriteVectorToStream(vec, col_meta);
            },
            column_variant);

        meta.columns_info.push_back(col_meta);
    }

    WriteBatchMetaBlock(meta);
}


void KioDbWriter::Finish() {
    if (is_finished_) {
        return;
    }
    db_file_.flush();
    db_file_.close();
    is_finished_ = true;
}


void KioDbWriter::WriteBatchMetaBlock(const kio::BatchMeta &meta) {
    const auto meta_start = db_file_.tellp();
    if (meta_start == std::ofstream::pos_type(-1)) {
        throw std::runtime_error("tellp() failed at meta_start");
    }

    db_file_.write(reinterpret_cast<const char *>(&meta.batch_id),
                   sizeof(meta.batch_id));

    std::uint64_t col_count =
            static_cast<std::uint64_t>(meta.columns_info.size());
    db_file_.write(reinterpret_cast<const char *>(&col_count),
                   sizeof(col_count));

    for (const auto &col: meta.columns_info) {
        db_file_.write(reinterpret_cast<const char *>(&col.type),
                       sizeof(col.type));
        db_file_.write(reinterpret_cast<const char *>(&col.offset),
                       sizeof(col.offset));
        db_file_.write(reinterpret_cast<const char *>(&col.size),
                       sizeof(col.size));
        db_file_.write(reinterpret_cast<const char *>(&col.count),
                       sizeof(col.count));
    }

    const auto meta_end = db_file_.tellp();
    if (meta_end == std::ofstream::pos_type(-1)) {
        throw std::runtime_error("tellp() failed at meta_end");
    }

    const std::uint64_t meta_size =
            static_cast<std::uint64_t>(meta_end - meta_start);

    // [meta_size][magic] — чтобы ридер мог с конца найти BatchMeta block
    db_file_.write(reinterpret_cast<const char *>(&meta_size),
                   sizeof(meta_size));
    db_file_.write(reinterpret_cast<const char *>(&kBatchMetaMagic),
                   sizeof(kBatchMetaMagic));

    if (!db_file_.good()) {
        throw std::runtime_error("Failed while writing batch meta/trailer");
    }
}


void KioDbWriter::WriteVectorToStream(const std::vector<std::int64_t> &vec,
                                      kio::ColumnChunkMeta &meta) {
    meta.count = static_cast<std::uint64_t>(vec.size());
    const std::uint64_t bytes =
            static_cast<std::uint64_t>(vec.size()) * sizeof(std::int64_t);

    if (bytes > 0) {
        db_file_.write(reinterpret_cast<const char *>(vec.data()),
                       static_cast<std::streamsize>(bytes));
    }
    meta.size = bytes;

    if (!db_file_.good()) {
        throw std::runtime_error("Failed to write int64 column data");
    }
}


void KioDbWriter::WriteVectorToStream(const std::vector<std::string> &vec,
                             kio::ColumnChunkMeta &meta) {
    meta.count = static_cast<std::uint64_t>(vec.size());
    const auto start = db_file_.tellp();
    if (start == std::ofstream::pos_type(-1)) {
        throw std::runtime_error("tellp() failed at string start");
    }

    for (const auto &s : vec) {
        std::uint32_t len = static_cast<std::uint32_t>(s.size());
        db_file_.write(reinterpret_cast<const char *>(&len), sizeof(len));
        if (len > 0) {
            db_file_.write(s.data(), len);
        }
    }

    const auto end = db_file_.tellp();
    if (end == std::ofstream::pos_type(-1)) {
        throw std::runtime_error("tellp() failed at string end");
    }

    meta.size = static_cast<std::uint64_t>(end - start);

    if (!db_file_.good()) {
        throw std::runtime_error("Failed to write string column data");
    }
}