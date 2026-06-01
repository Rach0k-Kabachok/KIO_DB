#include "execution/operators/sort_ops.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <queue>
#include <utility>
#include <vector>

#include "global/column_operations.h"

TopKOperator::TopKOperator(std::unique_ptr<IOperator> child_op,
                           const std::vector<SortKey>& sort_keys, size_t limit)
    : SortOperatorBase(sort_keys),
      child_op_(std::move(child_op)),
      limit_(limit) {
}

std::optional<ExecBatch> TopKOperator::Next() {
    if (done_ || limit_ == 0) {
        return std::nullopt;
    }
    done_ = true;

    std::optional<ExecBatch> optional_batch = child_op_->Next();
    if (!optional_batch.has_value()) {
        return std::nullopt;
    }

    std::shared_ptr<const Schema> output_schema = optional_batch->schema;
    const std::vector<SortColumn> sort_columns =
        MakeSortColumns(*output_schema);

    const auto heap_compare = [&sort_columns](const ctp::ColumnarBatch& lhs,
                                              const ctp::ColumnarBatch& rhs) {
        return SortOperatorBase::RowComesBefore(lhs, 0,
                                                rhs, 0,
                                                sort_columns);
    };
    std::priority_queue<ctp::ColumnarBatch, std::vector<ctp::ColumnarBatch>,
                        decltype(heap_compare)>
        top_k(heap_compare);

    do {
        ExecBatch& batch = optional_batch.value();

        for (size_t row_idx = 0; row_idx < batch.row_count; row_idx++) {
            if (top_k.size() < limit_) {
                top_k.push(
                    SortOperatorBase::MakeOutputColumnsByRowIds(batch, {row_idx}));
            } else if (SortOperatorBase::RowComesBefore(
                           batch.columns, row_idx,
                           top_k.top(), 0, sort_columns)) {
                top_k.pop();
                top_k.push(
                    SortOperatorBase::MakeOutputColumnsByRowIds(batch, {row_idx}));
            }
        }
    } while ((optional_batch = child_op_->Next()).has_value());

    std::vector<ctp::ColumnarBatch> rows;
    rows.reserve(top_k.size());
    while (!top_k.empty()) {
        rows.push_back(top_k.top());
        top_k.pop();
    }

    std::sort(rows.begin(), rows.end(),
              [&sort_columns](const ctp::ColumnarBatch& lhs, const ctp::ColumnarBatch& rhs) {
                  return SortOperatorBase::RowComesBefore(
                      lhs, 0, rhs, 0, sort_columns);
              });

    ctp::ColumnarBatch output_columns = ctp::MakeEmptyColumns(*output_schema);
    for (const ctp::ColumnarBatch& row : rows) {
        for (size_t col_idx = 0; col_idx < output_columns.size(); col_idx++) {
            ctp::AppendColumnValue(output_columns[col_idx],
                                   row[col_idx], 0);
        }
    }

    return ExecBatch{std::move(output_columns), std::move(output_schema),
                     rows.size()};
}
