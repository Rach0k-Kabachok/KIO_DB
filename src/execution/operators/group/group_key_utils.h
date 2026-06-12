#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "execution/operator.h"
#include "global/columnar_types.h"
#include "global/schema.h"

namespace exec_group {

size_t InitialCompactGroupReserve(size_t first_batch_rows);

ctp::ColumnarBatch MakeOutputColumns(
    const std::vector<Schema::Types>& output_types, size_t reserve_rows);

void FillEncodedGroupKey(const ExecBatch& batch,
                         const std::vector<size_t>& group_indices,
                         const std::vector<Schema::Types>& group_types,
                         size_t row_idx,
                         std::vector<uint64_t>& key);

bool StoredGroupKeyEqualsInput(
        const ctp::ColumnarBatch& group_key_columns,
        const ExecBatch& batch,
        const std::vector<size_t>& group_indices,
        const std::vector<Schema::Types>& group_types,
        size_t group_row,
        size_t input_row);

void AppendGroupKey(ctp::ColumnarBatch& group_key_columns,
                    const ExecBatch& batch,
                    const std::vector<size_t>& group_indices,
                    size_t row_idx);

}  // namespace exec_group
