#include "execution/operators.h"

#include <memory>
#include <optional>
#include <utility>

#include "global/column_operations.h"
#include "global/columnar_types.h"

FilterOperator::FilterOperator(std::unique_ptr<IOperator> child_op,
                   RowPredicate predicate):
                   child_op_(std::move(child_op)),
                   predicate_(std::move(predicate)) {
}

std::optional<ExecBatch> FilterOperator::Next() {
    std::optional<ExecBatch> exec_batch = child_op_->Next();
    
    if (!exec_batch.has_value()) {
        return std::nullopt;
    }

    ExecBatch& batch = exec_batch.value();
    const Schema& schema = *batch.schema;
    ctp::ColumnarBatch filtered_batch;
    filtered_batch.reserve(batch.columns.size());

    for (size_t col_idx = 0; col_idx < batch.columns.size(); col_idx++) {
        filtered_batch.push_back(
            ctp::MakeEmptyColumn(schema.ColumnType(col_idx), batch.row_count));
    }

    size_t row_count = 0;
    for (size_t row_idx = 0; row_idx < batch.row_count; row_idx++) {
        if (predicate_(batch, row_idx)) {
            for (size_t col_idx = 0; col_idx < filtered_batch.size();
                 col_idx++) {
                ctp::AppendColumnValue(
                    filtered_batch[col_idx],
                    batch.columns[col_idx],
                    row_idx,
                    schema.ColumnType(col_idx));
            }
            row_count++;
        }
    }

    return ExecBatch{std::move(filtered_batch), batch.schema, row_count};
}
