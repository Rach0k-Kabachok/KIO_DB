#include "execution/operators.h"

#include <cstddef>
#include <memory>
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

bool SortOperatorBase::CompareRows(
    const scalar::Row& lhs,
    const scalar::Row& rhs,
    const std::vector<SortColumn>& sort_columns) {
    for (const SortColumn& sort_column : sort_columns) {
        const int compare_result = scalar::Compare(
            lhs[sort_column.column_idx], rhs[sort_column.column_idx]);

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

scalar::Row SortOperatorBase::MakeRow(const ExecBatch& batch, size_t row_idx) {
    return scalar::MakeRow(batch.columns, row_idx);
}

std::vector<scalar::Row> SortOperatorBase::MakeRows(const ExecBatch& batch) {
    std::vector<scalar::Row> rows;
    rows.reserve(batch.row_count);

    for (size_t row_idx = 0; row_idx < batch.row_count; row_idx++) {
        rows.push_back(MakeRow(batch, row_idx));
    }

    return rows;
}

ctp::ColumnarBatch SortOperatorBase::MakeOutputColumns(
    const std::vector<scalar::Row>& rows,
    const std::shared_ptr<const Schema>& schema) {
    ctp::ColumnarBatch output_columns;
    output_columns.reserve(schema->ColumnCount());

    for (size_t col_idx = 0; col_idx < schema->ColumnCount(); col_idx++) {
        output_columns.push_back(
            ctp::MakeEmptyColumn(schema->ColumnType(col_idx), rows.size()));
    }

    for (const scalar::Row& row : rows) {
        for (size_t col_idx = 0; col_idx < output_columns.size(); col_idx++) {
            scalar::AppendValue(output_columns[col_idx], row[col_idx]);
        }
    }

    return output_columns;
}
