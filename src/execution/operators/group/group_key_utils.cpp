#include "execution/operators/group/group_key_utils.h"

#include <algorithm>
#include <bit>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "global/column_operations.h"

namespace exec_group {
namespace {

constexpr size_t kInitialCompactGroupReserve = 4096;

uint64_t EncodeGroupValue(const ctp::Column& column,
                          Schema::Types type,
                          size_t row_idx) {
    switch (type) {
    case Schema::BIGINT:
    case Schema::TIMESTAMP:
        return std::bit_cast<uint64_t>(
            std::get<std::vector<int64_t>>(column)[row_idx]);
    case Schema::INTEGER:
    case Schema::DATE:
        return std::bit_cast<uint32_t>(
            std::get<std::vector<int32_t>>(column)[row_idx]);
    case Schema::SMALLINT:
        return std::bit_cast<uint16_t>(
            std::get<std::vector<int16_t>>(column)[row_idx]);
    case Schema::TEXT:
    case Schema::VARCHAR:
        return static_cast<uint64_t>(
            std::hash<std::string_view>()(
                std::get<std::vector<std::string>>(column)[row_idx]));
    case Schema::CHAR:
        return static_cast<unsigned char>(
            std::get<std::vector<char>>(column)[row_idx]);
    case Schema::DOUBLE:
        return static_cast<uint64_t>(
            std::hash<double>()(
                std::get<std::vector<double>>(column)[row_idx]));
    }

    throw std::invalid_argument("Unsupported group column type");
}

bool ColumnValuesEqual(const ctp::Column& lhs,
                       const ctp::Column& rhs,
                       Schema::Types type,
                       size_t lhs_row,
                       size_t rhs_row) {
    switch (type) {
    case Schema::BIGINT:
    case Schema::TIMESTAMP:
        return std::get<std::vector<int64_t>>(lhs)[lhs_row] ==
               std::get<std::vector<int64_t>>(rhs)[rhs_row];
    case Schema::INTEGER:
    case Schema::DATE:
        return std::get<std::vector<int32_t>>(lhs)[lhs_row] ==
               std::get<std::vector<int32_t>>(rhs)[rhs_row];
    case Schema::SMALLINT:
        return std::get<std::vector<int16_t>>(lhs)[lhs_row] ==
               std::get<std::vector<int16_t>>(rhs)[rhs_row];
    case Schema::TEXT:
    case Schema::VARCHAR:
        return std::get<std::vector<std::string>>(lhs)[lhs_row] ==
               std::get<std::vector<std::string>>(rhs)[rhs_row];
    case Schema::CHAR:
        return std::get<std::vector<char>>(lhs)[lhs_row] ==
               std::get<std::vector<char>>(rhs)[rhs_row];
    case Schema::DOUBLE:
        return std::get<std::vector<double>>(lhs)[lhs_row] ==
               std::get<std::vector<double>>(rhs)[rhs_row];
    }

    throw std::invalid_argument("Unsupported group column type");
}

}  // namespace

size_t InitialCompactGroupReserve(size_t first_batch_rows) {
    return std::min(first_batch_rows, kInitialCompactGroupReserve);
}

ctp::ColumnarBatch MakeOutputColumns(
    const std::vector<Schema::Types>& output_types, size_t reserve_rows) {
    ctp::ColumnarBatch output_columns;
    output_columns.reserve(output_types.size());
    for (Schema::Types type : output_types) {
        output_columns.push_back(ctp::MakeEmptyColumn(type, reserve_rows));
    }
    return output_columns;
}

void FillEncodedGroupKey(const ExecBatch& batch,
                         const std::vector<size_t>& group_indices,
                         const std::vector<Schema::Types>& group_types,
                         size_t row_idx,
                         std::vector<uint64_t>& key) {
    for (size_t idx = 0; idx < group_indices.size(); idx++) {
        key[idx] = EncodeGroupValue(
            batch.columns[group_indices[idx]], group_types[idx], row_idx);
    }
}

bool StoredGroupKeyEqualsInput(
        const ctp::ColumnarBatch& group_key_columns,
        const ExecBatch& batch,
        const std::vector<size_t>& group_indices,
        const std::vector<Schema::Types>& group_types,
        size_t group_row,
        size_t input_row) {
    for (size_t idx = 0; idx < group_indices.size(); idx++) {
        if (!ColumnValuesEqual(group_key_columns[idx],
                               batch.columns[group_indices[idx]],
                               group_types[idx],
                               group_row,
                               input_row)) {
            return false;
        }
    }
    return true;
}

void AppendGroupKey(ctp::ColumnarBatch& group_key_columns,
                    const ExecBatch& batch,
                    const std::vector<size_t>& group_indices,
                    size_t row_idx) {
    for (size_t idx = 0; idx < group_indices.size(); idx++) {
        ctp::AppendColumnValue(group_key_columns[idx],
                               batch.columns[group_indices[idx]],
                               row_idx);
    }
}

}  // namespace exec_group
