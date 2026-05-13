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
    uint8_t type;
    uint64_t size;  // Schema::Types (INT64 = 0, STRING = 1)
};

struct BatchMeta {
    uint64_t batch_id;
    uint64_t row_num;
    uint64_t col_num;
    uint64_t batch_start_offset;
    uint64_t batch_size;
};
}  // namespace kio