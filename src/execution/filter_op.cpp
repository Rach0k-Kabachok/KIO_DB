#include "execution/operators.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "global/column_operations.h"
#include "global/columnar_types.h"

FilterOperator::FilterOperator(std::unique_ptr<IOperator> child_op,
                   RowPredicate predicate):
                   child_op_(std::move(child_op)),
                   predicate_(std::move(predicate)) {
}

std::optional<ExecBatch> FilterOperator::Next() {
    std::optional<ExecBatch> optional_batch = child_op_->Next();
    
    if (!optional_batch.has_value()) {
        return std::nullopt;
    }

    ExecBatch& exec_batch = optional_batch.value();
    std::vector<size_t> selected_rows;
    selected_rows.reserve(exec_batch.row_count);
    for (size_t row_idx = 0; row_idx < exec_batch.row_count; row_idx++) {
        if (predicate_(exec_batch, row_idx)) {
            selected_rows.push_back(row_idx);
        }
    }

    ctp::ColumnarBatch filtered_batch;
    filtered_batch.reserve(exec_batch.columns.size());
    for (const ctp::Column& column : exec_batch.columns) {
        filtered_batch.push_back(ctp::GatherColumnRows(column, selected_rows));
    }

    return ExecBatch{std::move(filtered_batch), exec_batch.schema,
                     selected_rows.size()};
}
