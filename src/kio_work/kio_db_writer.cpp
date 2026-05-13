#include "kio_work/kio_db_writer.h"

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

#include "column_operations.h"
#include "columnar_batch.h"
#include "columnar_types.h"
#include "kio_work/binary_io.h"
#include "kio_work/kio_format.h"
#include "kio_work/kio_serialization.h"
#include "schema.h"

KioDbWriter::KioDbWriter(const std::string& output_filename, const Schema& schema)
    : kio_name_(output_filename), schema_(schema) {

    kio_file_.open(kio_name_, std::ios::binary | std::ios::out);
    if (!kio_file_.is_open()) {
        throw std::runtime_error("Failed to open file: " + kio_name_);
    }

    WriteSchemaMeta();
}

void KioDbWriter::WriteSchemaMeta() {
    uint64_t column_count = static_cast<uint64_t>(schema_.GetColumnCount());
    bio::WritePod(kio_file_, column_count, "schema column count");

    for (uint64_t i = 0; i < column_count; i++) {
        const std::string& column_name = schema_.SearchNameByIndex(i);
        uint64_t name_size = static_cast<uint64_t>(column_name.size());
        uint8_t type = static_cast<uint8_t>(schema_.SearchTypeByIndex(i));

        bio::WritePod(kio_file_, name_size, "column name size");
        bio::WriteBytes(kio_file_, column_name.data(), name_size,
                        "column name");
        bio::WritePod(kio_file_, type, "column type");
    }
}

void KioDbWriter::WriteBatchMeta(const ctp::ColumnarBatch& batch) {
    if (batch.empty()) {
        return;
    }
    std::streampos batch_meta_pos = kio_file_.tellp();
    if (batch_meta_pos == std::streampos(-1)) {
        throw std::runtime_error("Failed to get batch metadata position");
    }

    kio::BatchMeta batch_meta{};
    batch_meta.batch_id = cur_batch_++;
    batch_meta.row_num = ctp::GetColumnRowCount(batch[0]);
    batch_meta.col_num = batch.size();
    batch_meta.batch_start_offset =
        static_cast<uint64_t>(batch_meta_pos) + sizeof(kio::BatchMeta);
    batch_meta.batch_size = kio::GetBatchPayloadSize(batch);

    bio::WritePod(kio_file_, batch_meta, "batch metadata");
}

void KioDbWriter::WriteColumns(const ctp::ColumnarBatch& batch) {
    for (size_t col_idx = 0; col_idx < batch.size(); ++col_idx) {
        const auto& column = batch[col_idx];
        Schema::Types col_type = schema_.SearchTypeByIndex(col_idx);

        auto [meta, payload] = kio::SerializeColumn(column, col_type);
        bio::WritePod(kio_file_, meta, "column chunk metadata");
        bio::WriteBytes(kio_file_, payload.data(), payload.size(), "column payload");
    }
}

void KioDbWriter::WriteBatchToFile(const ctp::ColumnarBatch& batch) {
    if (batch.empty()) {
        return;
    }

    ctp::ValidateColumnarBatch(batch, schema_);
    WriteBatchMeta(batch);
    WriteColumns(batch);
}
