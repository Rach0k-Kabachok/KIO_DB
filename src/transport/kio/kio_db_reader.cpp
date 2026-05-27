#include "transport/kio/kio_db_reader.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "global/columnar_types.h"
#include "global/schema.h"
#include "transport/compression/column_encoding.h"
#include "transport/kio/binary_io.h"
#include "transport/kio/kio_format.h"


KioDbReader::KioDbReader(const std::string& db_filename)
    : kio_file_(db_filename, std::ios::binary) {
    if (!kio_file_.is_open()) {
        throw std::runtime_error("Failed to open KIO file: " + db_filename);
    }

    ReadHeader();
    Reset();
}

const Schema& KioDbReader::GetSchema() const {
    return metadata_.schema;
}

const kio::FileMetadata& KioDbReader::GetMetadata() const {
    return metadata_;
}

const kio::RowGroupMeta& KioDbReader::GetRowGroupMeta() const {
    if (next_row_group_ == 0) {
        return metadata_.row_groups[0];
    }
    return metadata_.row_groups[next_row_group_ - 1];
}

const kio::RowGroupMeta* KioDbReader::PeekNextRowGroupMeta() const {
    if (next_row_group_ >= metadata_.row_groups.size()) {
        return nullptr;
    }
    return &metadata_.row_groups[next_row_group_];
}

std::optional<ctp::ColumnarBatch> KioDbReader::ReadNextBatch() {
    std::vector<size_t> column_indices;
    column_indices.reserve(metadata_.schema.ColumnCount());
    for (size_t i = 0; i < metadata_.schema.ColumnCount(); ++i) {
        column_indices.push_back(i);
    }
    return ReadNextBatch(column_indices);
}

std::optional<ctp::ColumnarBatch> KioDbReader::ReadNextBatch(
    const std::vector<size_t>& column_indices) {
    if (next_row_group_ >= metadata_.row_groups.size()) {
        return std::nullopt;
    }

    const kio::RowGroupMeta& row_group =
        metadata_.row_groups[next_row_group_++];
    ctp::ColumnarBatch batch;
    batch.reserve(column_indices.size());

    for (size_t col_idx : column_indices) {
        if (col_idx >= row_group.columns.size()) {
            throw std::out_of_range(
                "KIO projected column index is out of range");
        }

        const kio::ColumnChunkInfo& chunk = row_group.columns[col_idx];
        kio_file_.clear();
        kio_file_.seekg(static_cast<std::streamoff>(
            row_group.batch.batch_start_offset + chunk.local_offset));
        if (!kio_file_.good()) {
            throw std::runtime_error("Failed to seek KIO column chunk");
        }

        batch.emplace_back(
            ReadColumnFromFile(row_group.batch.row_num,
                               metadata_.schema.ColumnType(col_idx), chunk));
    }

    return batch;
}

void KioDbReader::SkipNextBatch() {
    if (next_row_group_ < metadata_.row_groups.size()) {
        next_row_group_++;
    }
}

void KioDbReader::Reset() {
    kio_file_.clear();
    next_row_group_ = 0;
}

void KioDbReader::ReadHeader() {
    std::string magic(sizeof(kio::kMagic), '\0');
    bio::ReadBytes(kio_file_, magic.data(), magic.size());
    const uint64_t footer_offset = bio::ReadPod<uint64_t>(kio_file_);

    if (!std::equal(std::begin(magic), std::end(magic),
                    std::begin(kio::kMagic))) {
        throw std::runtime_error("Invalid KIO file magic");
    }

    if (footer_offset == 0) {
        throw std::runtime_error("KIO file has no footer");
    }

    ReadFooter(footer_offset);
}

void KioDbReader::ReadFooter(uint64_t footer_offset) {
    kio_file_.seekg(static_cast<std::streamoff>(footer_offset));
    if (!kio_file_.good()) {
        throw std::runtime_error("Failed to seek KIO footer");
    }

    metadata_.row_count = bio::ReadPod<uint64_t>(kio_file_);

    metadata_.schema = ReadSchema();

    const uint64_t row_group_count = bio::ReadPod<uint64_t>(kio_file_);
    metadata_.row_groups.clear();
    metadata_.row_groups.reserve(row_group_count);

    for (uint64_t group_idx = 0; group_idx < row_group_count; ++group_idx) {
        metadata_.row_groups.push_back(
            ReadRowGroupMeta());
    }
}

Schema KioDbReader::ReadSchema() {
    const uint64_t column_count = bio::ReadPod<uint64_t>(kio_file_);
    std::vector<std::string> column_names;
    std::vector<Schema::Types> column_types;
    column_names.reserve(column_count);
    column_types.reserve(column_count);

    for (uint64_t i = 0; i < column_count; ++i) {
        column_names.push_back(bio::ReadString(kio_file_));
        const uint8_t type_id = bio::ReadPod<uint8_t>(kio_file_);
        column_types.push_back(static_cast<Schema::Types>(type_id));
    }

    return Schema::FromColumns(std::move(column_names),
                               std::move(column_types));
}

kio::ColumnChunkInfo KioDbReader::ReadColumnChunkInfo() {
    kio::ColumnChunkInfo chunk;
    chunk.local_offset = bio::ReadPod<uint64_t>(kio_file_);
    chunk.size = bio::ReadPod<uint64_t>(kio_file_);
    chunk.compressed_size = bio::ReadPod<uint64_t>(kio_file_);
    chunk.uncompressed_size = bio::ReadPod<uint64_t>(kio_file_);
    chunk.encoding =
        static_cast<kio::Encoding>(bio::ReadPod<uint8_t>(kio_file_));
    chunk.compression =
        static_cast<kio::Compression>(bio::ReadPod<uint8_t>(kio_file_));
    chunk.has_min_max = bio::ReadPod<uint8_t>(kio_file_) != 0;
    chunk.min_value = bio::ReadString(kio_file_);
    chunk.max_value = bio::ReadString(kio_file_);
    return chunk;
}

kio::RowGroupMeta KioDbReader::ReadRowGroupMeta() {
    kio::RowGroupMeta row_group;
    row_group.batch = bio::ReadPod<kio::BatchMeta>(kio_file_);

    const uint64_t chunk_count = bio::ReadPod<uint64_t>(kio_file_);
    if (chunk_count != metadata_.schema.ColumnCount()) {
        throw std::runtime_error(
            "KIO footer chunk count does not match schema");
    }

    row_group.columns.reserve(chunk_count);
    for (uint64_t chunk_idx = 0; chunk_idx < chunk_count; ++chunk_idx) {
        row_group.columns.push_back(ReadColumnChunkInfo());
    }

    return row_group;
}

ctp::Column KioDbReader::ReadColumnFromFile(uint64_t row_num,
                               Schema::Types type,
                               const kio::ColumnChunkInfo& chunk) {
    if (chunk.size != chunk.compressed_size) {
        throw std::runtime_error("Corrupted KIO footer: chunk size mismatch");
    }

    std::vector<char> payload(chunk.size);
    bio::ReadBytes(kio_file_, payload.data(), chunk.size);
    return DecodeColumnForRead(
        payload, type, chunk.encoding, chunk.compression, row_num,
        chunk.uncompressed_size);
}
