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
#include "transport/kio/binary_io.h"
#include "transport/kio/kio_format.h"

namespace {
std::string ReadString(std::istream& input) {
    const uint64_t size = bio::ReadPod<uint64_t>(input);
    std::string value(static_cast<size_t>(size), '\0');
    bio::ReadBytes(input, value.data(), size);
    return value;
}

ctp::Column ReadStringColumn(std::istream& input, uint64_t row_num,
                             const kio::ColumnChunkMeta& chunk_meta) {
    std::vector<std::string> result;
    result.reserve(row_num);

    std::vector<uint64_t> str_sizes(row_num);
    const uint64_t sizes_bytes = row_num * sizeof(uint64_t);
    bio::ReadBytes(input, reinterpret_cast<char*>(str_sizes.data()),
                   sizes_bytes);

    uint64_t strings_size = 0;
    for (const auto& sz : str_sizes) {
        strings_size += sz;
    }
    if (sizes_bytes + strings_size != chunk_meta.size) {
        throw std::runtime_error(
            "Corrupted KIO batch: string column size mismatch");
    }

    for (uint64_t size : str_sizes) {
        std::string value(size, '\0');
        bio::ReadBytes(input, value.data(), size);
        result.push_back(std::move(value));
    }

    return result;
}

template <typename T>
ctp::Column ReadFixedColumn(std::istream& input, uint64_t row_num,
                            const kio::ColumnChunkMeta& chunk_meta) {
    const uint64_t expected_size = row_num * sizeof(T);
    if (chunk_meta.size != expected_size) {
        throw std::runtime_error(
            "Corrupted KIO batch: fixed column size mismatch");
    }

    std::vector<T> result(row_num);
    bio::ReadBytes(input, reinterpret_cast<char*>(result.data()),
                   expected_size);
    return result;
}

ctp::Column ReadColumnByType(std::istream& input, uint64_t row_num,
                             const kio::ColumnChunkMeta& chunk_meta,
                             Schema::Types type) {
    switch (type) {
        case Schema::BIGINT:
        case Schema::TIMESTAMP:
            return ReadFixedColumn<int64_t>(input, row_num, chunk_meta);
        case Schema::INTEGER:
        case Schema::DATE:
            return ReadFixedColumn<int32_t>(input, row_num, chunk_meta);
        case Schema::SMALLINT:
            return ReadFixedColumn<int16_t>(input, row_num, chunk_meta);
        case Schema::CHAR:
            return ReadFixedColumn<char>(input, row_num, chunk_meta);
        case Schema::TEXT:
        case Schema::VARCHAR:
            return ReadStringColumn(input, row_num, chunk_meta);
    }

    throw std::runtime_error("Unsupported schema type");
}

ctp::Column ReadColumnFromFile(std::ifstream& file, uint64_t row_num,
                               Schema::Types type) {
    const kio::ColumnChunkMeta chunk_meta =
        bio::ReadPod<kio::ColumnChunkMeta>(file);

    return ReadColumnByType(file, row_num, chunk_meta, type);
}

kio::Encoding ReadEncoding(std::istream& input) {
    const uint8_t encoding = bio::ReadPod<uint8_t>(input);
    if (encoding != static_cast<uint8_t>(kio::Encoding::PLAIN)) {
        throw std::runtime_error("Unsupported KIO encoding");
    }
    return kio::Encoding::PLAIN;
}

kio::Compression ReadCompression(std::istream& input) {
    const uint8_t compression = bio::ReadPod<uint8_t>(input);
    if (compression != static_cast<uint8_t>(kio::Compression::NONE)) {
        throw std::runtime_error("Unsupported KIO compression");
    }
    return kio::Compression::NONE;
}
}  // namespace

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

const kio::RowGroupMeta& KioDbReader::GetRowGroupMeta(size_t group_idx) const {
    return metadata_.row_groups[group_idx];
}


void KioDbReader::Reset() {
    kio_file_.clear();
    next_row_group_ = 0;
}

void KioDbReader::ReadHeader() {
    char magic[sizeof(kio::kMagic)]{};
    bio::ReadBytes(kio_file_, magic, sizeof(magic));
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

    const uint64_t column_count = bio::ReadPod<uint64_t>(kio_file_);
    std::vector<std::string> column_names;
    std::vector<Schema::Types> column_types;
    column_names.reserve(column_count);
    column_types.reserve(column_count);

    for (uint64_t i = 0; i < column_count; ++i) {
        column_names.push_back(ReadString(kio_file_));
        const uint8_t type_id = bio::ReadPod<uint8_t>(kio_file_);
        column_types.push_back(static_cast<Schema::Types>(type_id));
    }
    metadata_.schema =
        Schema::FromColumns(std::move(column_names), std::move(column_types));

    const uint64_t row_group_count = bio::ReadPod<uint64_t>(kio_file_);
    metadata_.row_groups.clear();
    metadata_.row_groups.reserve(static_cast<size_t>(row_group_count));

    for (uint64_t group_idx = 0; group_idx < row_group_count; ++group_idx) {
        kio::RowGroupMeta row_group;
        row_group.batch = bio::ReadPod<kio::BatchMeta>(kio_file_);

        const uint64_t chunk_count = bio::ReadPod<uint64_t>(kio_file_);
        if (chunk_count != metadata_.schema.ColumnCount()) {
            throw std::runtime_error(
                "KIO footer chunk count does not match schema");
        }

        row_group.columns.reserve(static_cast<size_t>(chunk_count));
        for (uint64_t chunk_idx = 0; chunk_idx < chunk_count; ++chunk_idx) {
            kio::ColumnChunkInfo chunk;
            chunk.local_offset = bio::ReadPod<uint64_t>(kio_file_);
            chunk.compressed_size = bio::ReadPod<uint64_t>(kio_file_);
            chunk.uncompressed_size = bio::ReadPod<uint64_t>(kio_file_);
            chunk.encoding = ReadEncoding(kio_file_);
            chunk.compression = ReadCompression(kio_file_);
            chunk.has_min_max = bio::ReadPod<uint8_t>(kio_file_) != 0;
            chunk.min_value = ReadString(kio_file_);
            chunk.max_value = ReadString(kio_file_);
            row_group.columns.push_back(std::move(chunk));
        }

        metadata_.row_groups.push_back(std::move(row_group));
    }
}

std::optional<KioReadBatch> KioDbReader::ReadNextBatch() {
    std::vector<size_t> column_indices;
    column_indices.reserve(metadata_.schema.ColumnCount());
    for (size_t i = 0; i < metadata_.schema.ColumnCount(); ++i) {
        column_indices.push_back(i);
    }
    return ReadNextBatch(column_indices);
}

std::optional<KioReadBatch> KioDbReader::ReadNextBatch(
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
            ReadColumnFromFile(kio_file_, row_group.batch.row_num,
                               metadata_.schema.ColumnType(col_idx)));
    }

    return KioReadBatch{std::move(batch), row_group.batch.row_num};
}

void KioDbReader::SkipNextBatch() {
    if (next_row_group_ < metadata_.row_groups.size()) {
        next_row_group_++;
    }
}
