#pragma once

#include <vector>
#include <string>
#include <variant>
#include <cstdint>

namespace ctp {
using ParsedRow = std::vector<std::string>;
using ParsedBatch = std::vector<std::vector<std::string> >;
using Column = std::variant<std::vector<int64_t>, std::vector<std::string> >;
using ColumnarBatch = std::vector<Column>;
}  // namespace ctp

namespace kio {
struct ColumnChunkMeta {
    uint8_t type;  // Schema::Types (INT64 = 0, STRING = 1)
    uint64_t offset;
    uint64_t size;
    uint64_t count;
};

struct BatchMeta {
    uint64_t batch_id;
    std::vector<ColumnChunkMeta> columns_info;
};
}  // namespace kio