#pragma once

#include <cstdint>

namespace kio {
struct ColumnChunkMeta {
    uint64_t size;
};

struct BatchMeta {
    uint64_t batch_id;
    uint64_t row_num;
    uint64_t col_num;
    uint64_t batch_start_offset;
    uint64_t batch_size;
};
}  // namespace kio
