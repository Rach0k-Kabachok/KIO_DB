#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "global/schema.h"

namespace kio {
inline constexpr char kMagic[4] = {'K', 'I', 'O', 'D'};

enum class Encoding : uint8_t {
    PLAIN = 0,
};

enum class Compression : uint8_t {
    NONE = 0,
};

struct ColumnChunkInfo {
    uint64_t local_offset = 0;
    uint64_t size = 0;
    uint64_t compressed_size = 0;
    uint64_t uncompressed_size = 0;
    Encoding encoding = Encoding::PLAIN;
    Compression compression = Compression::NONE;
    bool has_min_max = false;
    std::string min_value;
    std::string max_value;
};

struct BatchMeta {
    uint64_t batch_id = 0;
    uint64_t row_num = 0;
    uint64_t col_num = 0;
    uint64_t batch_start_offset = 0;
    uint64_t batch_size = 0;
};

struct RowGroupMeta {
    BatchMeta batch;
    std::vector<ColumnChunkInfo> columns;
};

struct FileMetadata {
    Schema schema;
    uint64_t row_count = 0;
    std::vector<RowGroupMeta> row_groups;
};
}  // namespace kio
