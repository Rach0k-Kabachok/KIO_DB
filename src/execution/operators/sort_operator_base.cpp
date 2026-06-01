#include "execution/operators/sort_ops.h"

#include <cstddef>
#include <vector>

#include "global/column_operations.h"
#include "global/scalar_value.h"

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

bool SortOperatorBase::RowComesBefore(
    const ctp::ColumnarBatch& lhs_columns,
    size_t lhs_row,
    const ctp::ColumnarBatch& rhs_columns,
    size_t rhs_row,
    const std::vector<SortColumn>& sort_columns) {
    for (const SortColumn& sort_column : sort_columns) {
        const ctp::Column& lhs_column = lhs_columns[sort_column.column_idx];
        const ctp::Column& rhs_column = rhs_columns[sort_column.column_idx];

        const int compare_result = scalar::Compare(
            scalar::GetValue(lhs_column, lhs_row),
            scalar::GetValue(rhs_column, rhs_row));

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

ctp::ColumnarBatch SortOperatorBase::MakeOutputColumnsByRowIds(
    const ExecBatch& batch, const std::vector<size_t>& row_ids) {
    ctp::ColumnarBatch output_columns;
    output_columns.reserve(batch.columns.size());

    for (const ctp::Column& column : batch.columns) {
        output_columns.push_back(ctp::GatherColumnRows(column, row_ids));
    }

    return output_columns;
}
