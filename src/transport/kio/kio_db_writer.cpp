#include "transport/kio/kio_db_writer.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "column_operations.h"
#include "columnar_types.h"
#include "transport/compression/column_encoding.h"
#include "transport/kio/binary_io.h"
#include "transport/kio/kio_format.h"
#include "schema.h"

namespace {
constexpr std::streamoff kFooterOffsetPosition =
    sizeof(kio::kMagic);

struct SerializedColumnChunk {
    kio::ColumnChunkInfo info;
    std::vector<char> payload;
};

void WriteString(std::ostream& output, const std::string& value) {
    const uint64_t size = value.size();
    bio::WritePod(output, size);
    bio::WriteBytes(output, value.data(), size);
}

template <typename T>
std::string FormatStatsValue(T value) {
    if constexpr (std::is_floating_point_v<T>) {
        std::ostringstream out;
        out << std::setprecision(17) << value;
        return out.str();
    } else {
        return std::to_string(value);
    }
}

template <typename T>
kio::ColumnChunkInfo FindMinMax(const std::vector<T>& values) {
    kio::ColumnChunkInfo info;
    if (values.empty()) {
        return info;
    }

    auto [min_it, max_it] = std::minmax_element(values.begin(), values.end());
    info.has_min_max = true;

    if constexpr (std::is_same_v<T, std::string>) {
        info.min_value = *min_it;
        info.max_value = *max_it;
    } else if constexpr (std::is_same_v<T, char>) {
        info.min_value = std::string(1, *min_it);
        info.max_value = std::string(1, *max_it);
    } else {
        info.min_value = FormatStatsValue(*min_it);
        info.max_value = FormatStatsValue(*max_it);
    }

    return info;
}

kio::ColumnChunkInfo MakeColumnInfo(const ctp::Column& column,
                                    uint64_t local_offset,
                                    const PreparedColumn& prepared) {
    kio::ColumnChunkInfo info = std::visit(
        [](const auto& values) { return FindMinMax(values); }, column);

    info.local_offset = local_offset;
    info.size = prepared.payload.size();
    info.compressed_size = prepared.compressed_size;
    info.uncompressed_size = prepared.uncompressed_size;
    info.encoding = prepared.encoding;
    info.compression = prepared.compression;
    return info;
}

uint64_t GetEncodedBatchSize(
    const std::vector<kio::ColumnChunkInfo>& columns) {
    uint64_t result = 0;
    for (const auto& column : columns) {
        result += column.size;
    }
    return result;
}
}  // namespace

KioDbWriter::KioDbWriter(const std::string& output_filename, const Schema& schema)
    : metadata_{schema, 0, {}} {

    kio_file_.open(output_filename, std::ios::binary | std::ios::out);
    if (!kio_file_.is_open()) {
        throw std::runtime_error("Failed to open file: " + output_filename);
    }

    WriteHeader();
}

KioDbWriter::~KioDbWriter() {
    try {
        Finalize();
    } catch (...) {
    }
}

void KioDbWriter::WriteHeader() {
    const uint64_t footer_offset = 0;
    bio::WriteBytes(kio_file_, kio::kMagic, sizeof(kio::kMagic));
    bio::WritePod(kio_file_, footer_offset);
}

kio::BatchMeta KioDbWriter::MakeBatchMeta(const ctp::ColumnarBatch& batch) {
    std::streampos batch_start_pos = kio_file_.tellp();
    if (batch_start_pos == std::streampos(-1)) {
        throw std::runtime_error("Failed to get batch start position");
    }

    kio::BatchMeta batch_meta{};
    batch_meta.batch_id = cur_batch_++;
    batch_meta.row_num = ctp::GetColumnRowCount(batch[0]);
    batch_meta.col_num = batch.size();
    batch_meta.batch_start_offset = static_cast<uint64_t>(batch_start_pos);
    batch_meta.batch_size = 0;

    return batch_meta;
}

std::vector<kio::ColumnChunkInfo> KioDbWriter::WriteColumns(
    const ctp::ColumnarBatch& batch) {
    std::vector<SerializedColumnChunk> chunks;
    std::vector<kio::ColumnChunkInfo> column_infos;
    chunks.reserve(batch.size());
    column_infos.reserve(batch.size());

    uint64_t column_offset = 0;

    for (size_t col_idx = 0; col_idx < batch.size(); ++col_idx) {
        const auto& column = batch[col_idx];
        const Schema::Types col_type = metadata_.schema.ColumnType(col_idx);

        PreparedColumn prepared =
            PrepareColumnForWrite(column, col_type);
        SerializedColumnChunk chunk;
        chunk.info = MakeColumnInfo(column, column_offset, prepared);
        chunk.payload = std::move(prepared.payload);
        column_offset += chunk.info.size;
        column_infos.push_back(std::move(chunk.info));
        chunks.push_back(std::move(chunk));
    }

    for (const auto& chunk : chunks) {
        bio::WriteBytes(kio_file_, chunk.payload.data(), chunk.payload.size());
    }

    return column_infos;
}

void KioDbWriter::WriteBatchToFile(const ctp::ColumnarBatch& batch) {
    if (finalized_) {
        throw std::runtime_error("Cannot write to finalized KIO file");
    }

    if (batch.empty()) {
        return;
    }

    kio::RowGroupMeta row_group;
    row_group.batch = MakeBatchMeta(batch);
    row_group.columns = WriteColumns(batch);
    row_group.batch.batch_size = GetEncodedBatchSize(row_group.columns);

    metadata_.row_count += row_group.batch.row_num;
    metadata_.row_groups.push_back(std::move(row_group));
}

void KioDbWriter::WriteFooter() {
    bio::WritePod(kio_file_, metadata_.row_count);

    const uint64_t column_count = metadata_.schema.ColumnCount();
    bio::WritePod(kio_file_, column_count);
    for (uint64_t i = 0; i < column_count; ++i) {
        WriteString(kio_file_, metadata_.schema.ColumnName(i));
        const uint8_t type = metadata_.schema.ColumnType(i);
        bio::WritePod(kio_file_, type);
    }

    const uint64_t row_group_count = metadata_.row_groups.size();
    bio::WritePod(kio_file_, row_group_count);
    for (const auto& row_group : metadata_.row_groups) {
        bio::WritePod(kio_file_, row_group.batch);

        const uint64_t chunk_count = row_group.columns.size();
        bio::WritePod(kio_file_, chunk_count);
        for (const auto& chunk : row_group.columns) {
            bio::WritePod(kio_file_, chunk.local_offset);
            bio::WritePod(kio_file_, chunk.size);
            bio::WritePod(kio_file_, chunk.compressed_size);
            bio::WritePod(kio_file_, chunk.uncompressed_size);

            const uint8_t encoding = static_cast<uint8_t>(chunk.encoding);
            const uint8_t compression = static_cast<uint8_t>(chunk.compression);
            const uint8_t has_min_max = chunk.has_min_max ? 1 : 0;
            bio::WritePod(kio_file_, encoding);
            bio::WritePod(kio_file_, compression);
            bio::WritePod(kio_file_, has_min_max);
            WriteString(kio_file_, chunk.min_value);
            WriteString(kio_file_, chunk.max_value);
        }
    }
}

void KioDbWriter::Finalize() {
    if (finalized_ || !kio_file_.is_open()) {
        return;
    }

    const std::streampos footer_pos = kio_file_.tellp();
    if (footer_pos == std::streampos(-1)) {
        throw std::runtime_error("Failed to get footer position");
    }

    const uint64_t footer_offset = static_cast<uint64_t>(footer_pos);
    WriteFooter();

    const std::streampos end_pos = kio_file_.tellp();
    kio_file_.seekp(kFooterOffsetPosition);
    bio::WritePod(kio_file_, footer_offset);
    kio_file_.seekp(end_pos);
    kio_file_.flush();
    finalized_ = true;
}
