#include "execution/operators.h"

#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "global/column_operations.h"

SortOperatorBase::SortOperatorBase(const std::vector<SortKey>& sort_keys)
    : sort_keys_(sort_keys) {
}

std::vector<SortOperatorBase::SortColumn> SortOperatorBase::MakeSortColumns(
    const Schema& schema) const {
    std::vector<SortColumn> sort_columns;
    sort_columns.reserve(sort_keys_.size());

    for (const SortKey& sort_key : sort_keys_) {
        const size_t column_idx = schema.ColumnIndex(sort_key.column_name);
        sort_columns.push_back(SortColumn{column_idx, sort_key.order});
    }

    return sort_columns;
}

bool SortOperatorBase::CompareRows(
    const VarVector& lhs,
    const VarVector& rhs,
    const std::vector<SortColumn>& sort_columns) {
    for (const SortColumn& sort_column : sort_columns) {
        const int compare_result = std::visit(
            [&rhs, &sort_column](const auto& lhs_value) -> int {
                using Value = std::decay_t<decltype(lhs_value)>;
                const auto& rhs_value =
                    std::get<Value>(rhs[sort_column.column_idx]);

                if (lhs_value == rhs_value) {
                    return 0;
                }

                return lhs_value < rhs_value ? -1 : 1;
            },
            lhs[sort_column.column_idx]);

        if (compare_result == 0) {
            continue;
        }

        if (sort_column.order == SortOrder::ASC) {
            return compare_result < 0;
        }
        return compare_result > 0;
    }

    return false;
}

VarVector SortOperatorBase::MakeRow(const ExecBatch& batch, size_t row_idx) {
    VarVector row;
    row.reserve(batch.columns.size());

    for (const ctp::Column& column : batch.columns) {
        std::visit(
            [&row, row_idx](const auto& values) {
                row.push_back(values[row_idx]);
            },
            column);
    }

    return row;
}

std::vector<VarVector> SortOperatorBase::MakeRows(const ExecBatch& batch) {
    std::vector<VarVector> rows;
    rows.reserve(batch.row_count);

    for (size_t row_idx = 0; row_idx < batch.row_count; row_idx++) {
        rows.push_back(MakeRow(batch, row_idx));
    }

    return rows;
}

ctp::ColumnarBatch SortOperatorBase::MakeOutputColumns(
    const std::vector<VarVector>& rows,
    const std::shared_ptr<const Schema>& schema) {
    ctp::ColumnarBatch output_columns;
    output_columns.reserve(schema->ColumnCount());

    for (size_t col_idx = 0; col_idx < schema->ColumnCount(); col_idx++) {
        output_columns.push_back(
            ctp::MakeEmptyColumn(schema->ColumnType(col_idx), rows.size()));
    }

    for (const VarVector& row : rows) {
        for (size_t col_idx = 0; col_idx < output_columns.size(); col_idx++) {
            std::visit(
                [&row, col_idx](auto& values) {
                    using Value =
                        typename std::decay_t<decltype(values)>::value_type;
                    values.push_back(std::get<Value>(row[col_idx]));
                },
                output_columns[col_idx]);
        }
    }

    return output_columns;
}

