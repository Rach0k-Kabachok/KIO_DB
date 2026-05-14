#include "transport/kio/kio_db_reader.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

#include "columnar_batch.h"
#include "columnar_types.h"
#include "transport/kio/binary_io.h"
#include "transport/kio/kio_format.h"
#include "schema.h"

namespace {
kio::ColumnChunkMeta ReadColumnChunkMeta(const std::string& batch, size_t& pos) {
    return bio::ReadPodFromBuffer<kio::ColumnChunkMeta>(
        batch, pos, "column chunk metadata");
}

ctp::Column ReadStringColumn(size_t& pos, uint64_t row_num,
                             const kio::ColumnChunkMeta& chunk_meta,
                             const std::string& batch) {
    std::vector<std::string> result;
    result.reserve(row_num);

    std::vector<uint64_t> str_sizes(row_num);
    uint64_t sizes_bytes = row_num * sizeof(uint64_t);
    bio::EnsureAvailable(batch, pos, sizes_bytes, "string sizes");
    std::memcpy(str_sizes.data(), batch.data() + pos, sizes_bytes);
    pos += sizes_bytes;

    uint64_t strings_size = 0;
    for (const auto& sz : str_sizes) {
        strings_size += sz;
        bio::EnsureAvailable(batch, pos, sz, "string data");
        result.emplace_back(batch.data() + pos, sz);
        pos += sz;
    }
    if (sizes_bytes + strings_size != chunk_meta.size) {
        throw std::runtime_error("Corrupted KIO batch: string column size mismatch");
    }

    return result;
}

template <typename T>
ctp::Column ReadFixedColumn(size_t& pos, uint64_t row_num,
                            const kio::ColumnChunkMeta& chunk_meta,
                            const std::string& batch) {
    uint64_t expected_size = row_num * sizeof(T);
    if (chunk_meta.size != expected_size) {
        throw std::runtime_error("Corrupted KIO batch: fixed column size mismatch");
    }

    std::vector<T> result(row_num);
    bio::EnsureAvailable(batch, pos, expected_size, "fixed column data");
    std::memcpy(result.data(), batch.data() + pos, expected_size);
    pos += expected_size;
    return result;
}

ctp::Column ReadColumnByType(size_t& pos, uint64_t row_num,
                             const kio::ColumnChunkMeta& chunk_meta,
                             const std::string& batch, Schema::Types type) {
    switch (type) {
    case Schema::BIGINT:
    case Schema::TIMESTAMP:
        return ReadFixedColumn<int64_t>(pos, row_num, chunk_meta, batch);
    case Schema::INTEGER:
    case Schema::DATE:
        return ReadFixedColumn<int32_t>(pos, row_num, chunk_meta, batch);
    case Schema::SMALLINT:
        return ReadFixedColumn<int16_t>(pos, row_num, chunk_meta, batch);
    case Schema::CHAR:
        return ReadFixedColumn<char>(pos, row_num, chunk_meta, batch);
    case Schema::TEXT:
    case Schema::VARCHAR:
        return ReadStringColumn(pos, row_num, chunk_meta, batch);
    }

    throw std::runtime_error("Unsupported schema type");
}

ctp::Column ReadColumnFromFile(std::ifstream& file, uint64_t row_num,
                               Schema::Types type) {
    kio::ColumnChunkMeta chunk_meta =
        bio::ReadPod<kio::ColumnChunkMeta>(file, "column chunk metadata");

    std::string column_payload(chunk_meta.size, '\0');
    bio::ReadBytes(file, column_payload.data(), column_payload.size(),
                   "column payload");

    size_t pos = 0;
    return ReadColumnByType(pos, row_num, chunk_meta, column_payload, type);
}

}  // namespace

KioDbReader::KioDbReader(const std::string& db_filename,
                         const Schema& schema):
    kio_file_(db_filename, std::ios::binary), schema_(schema) {
    if (!kio_file_.is_open()) {
        throw std::runtime_error("Failed to open KIO file: " + db_filename);
    }

    ReadSchemaMeta();
    batches_start_pos_ = kio_file_.tellg();
    if (batches_start_pos_ == std::streampos(-1)) {
        throw std::runtime_error("Failed to remember batches start position");
    }
}

const Schema& KioDbReader::GetSchema() const {
    return schema_;
}

void KioDbReader::Reset() {
    kio_file_.clear();
    kio_file_.seekg(batches_start_pos_);
    if (!kio_file_.good()) {
        throw std::runtime_error("Failed to reset KIO reader");
    }
}

void KioDbReader::ReadSchemaMeta() {
    if (kio_file_.peek() == std::ifstream::traits_type::eof()) {
        kio_file_.clear();
        return;
    }

    uint64_t column_count = bio::ReadPod<uint64_t>(
        kio_file_, "schema column count");

    if (column_count != schema_.GetColumnCount()) {
        throw std::runtime_error("KIO schema column count does not match schema");
    }

    for (uint64_t i = 0; i < column_count; i++) {
        uint64_t name_size = bio::ReadPod<uint64_t>(
            kio_file_, "column name size");

        std::string column_name(static_cast<size_t>(name_size), '\0');
        bio::ReadBytes(kio_file_, column_name.data(), name_size, "column name");

        uint8_t type_id = bio::ReadPod<uint8_t>(kio_file_, "column type");
        Schema::Types type = Schema::TypeFromId(type_id);

        if (column_name != schema_.SearchNameByIndex(i) ||
            type != schema_.SearchTypeByIndex(i)) {
            throw std::runtime_error("KIO schema metadata does not match schema");
        }
    }
}

ctp::ColumnarBatch KioDbReader::ReadNextBatch() {
    kio::BatchMeta batch_meta{};
    if (!bio::TryReadPod(kio_file_, batch_meta, "batch metadata")) {
        return {};
    }

    if (batch_meta.col_num != schema_.GetColumnCount()) {
        throw std::runtime_error("KIO batch column count does not match schema");
    }

    ctp::ColumnarBatch batch;
    batch.reserve(static_cast<size_t>(batch_meta.col_num));

    std::string readed_batch(batch_meta.batch_size, '\0');
    bio::ReadBytes(kio_file_, readed_batch.data(), readed_batch.size(),
                   "batch data");

    size_t pos = 0;
    std::vector<uint64_t> column_offsets;
    column_offsets.reserve(static_cast<size_t>(batch_meta.col_num));
    for (size_t col = 0; col < batch_meta.col_num; col++) {
        column_offsets.push_back(
            bio::ReadPodFromBuffer<uint64_t>(readed_batch, pos, "column offset"));
    }

    for (size_t col = 0; col < batch_meta.col_num; col++) {
        if (column_offsets[col] > readed_batch.size()) {
            throw std::runtime_error("Corrupted KIO batch: column offset is out of bounds");
        }

        size_t column_pos = static_cast<size_t>(column_offsets[col]);
        kio::ColumnChunkMeta chunk_meta =
            ReadColumnChunkMeta(readed_batch, column_pos);

        if (chunk_meta.size > readed_batch.size() - column_pos) {
            throw std::runtime_error("Corrupted KIO batch: column chunk is out of bounds");
        }

        const size_t payload_start = column_pos;
        batch.emplace_back(ReadColumnByType(
            column_pos, batch_meta.row_num, chunk_meta, readed_batch,
            schema_.SearchTypeByIndex(col)));

        if (column_pos != payload_start + chunk_meta.size) {
            throw std::runtime_error("Corrupted KIO batch: column size mismatch");
        }
    }

    if (pos > readed_batch.size()) {
        throw std::runtime_error("Corrupted KIO batch: payload size mismatch");
    }

    ctp::ValidateColumnarBatch(batch, schema_);
    return batch;
}

ctp::ColumnarBatch KioDbReader::ReadNextProjectedBatch(
    const std::vector<size_t>& column_indices) {
    kio::BatchMeta batch_meta{};
    if (!bio::TryReadPod(kio_file_, batch_meta, "batch metadata")) {
        return {};
    }

    if (batch_meta.col_num != schema_.GetColumnCount()) {
        throw std::runtime_error("KIO batch column count does not match schema");
    }

    std::vector<uint64_t> column_offsets;
    column_offsets.reserve(static_cast<size_t>(batch_meta.col_num));
    for (size_t col = 0; col < batch_meta.col_num; col++) {
        column_offsets.push_back(
            bio::ReadPod<uint64_t>(kio_file_, "column offset"));
    }

    ctp::ColumnarBatch batch;
    batch.reserve(column_indices.size());

    for (size_t col_idx : column_indices) {
        Schema::Types type = schema_.SearchTypeByIndex(col_idx);
        kio_file_.seekg(batch_meta.batch_start_offset + column_offsets[col_idx]);
        if (!kio_file_.good()) {
            throw std::runtime_error("Failed to seek column chunk");
        }

        batch.emplace_back(
            ReadColumnFromFile(kio_file_, batch_meta.row_num, type));
    }

    kio_file_.seekg(batch_meta.batch_start_offset + batch_meta.batch_size);
    if (!kio_file_.good()) {
        throw std::runtime_error("Failed to seek next batch");
    }

    return batch;
}
