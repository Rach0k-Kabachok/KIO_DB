#include "execution/operators.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <queue>
#include <utility>
#include <vector>

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

    const auto heap_compare = [&sort_columns](const scalar::Row& lhs,
                                              const scalar::Row& rhs) {
        return SortOperatorBase::CompareRows(lhs, rhs, sort_columns);
    };
    std::priority_queue<scalar::Row, std::vector<scalar::Row>,
                        decltype(heap_compare)>
        top_k(heap_compare);

    do {
        ExecBatch& batch = optional_batch.value();

        for (size_t row_idx = 0; row_idx < batch.row_count; row_idx++) {
            scalar::Row row = SortOperatorBase::MakeRow(batch, row_idx);

            if (top_k.size() < limit_) {
                top_k.push(std::move(row));
            } else if (SortOperatorBase::CompareRows(row, top_k.top(),
                                                     sort_columns)) {
                top_k.pop();
                top_k.push(std::move(row));
            }
        }
    } while ((optional_batch = child_op_->Next()).has_value());

    std::vector<scalar::Row> rows;
    rows.reserve(top_k.size());
    while (!top_k.empty()) {
        rows.push_back(top_k.top());
        top_k.pop();
    }

    std::sort(rows.begin(), rows.end(),
              [&sort_columns](const scalar::Row& lhs,
                              const scalar::Row& rhs) {
                  return SortOperatorBase::CompareRows(lhs, rhs, sort_columns);
              });

    return ExecBatch{MakeOutputColumns(rows, output_schema), std::move(output_schema),
                     rows.size()};
}
