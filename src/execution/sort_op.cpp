#include "execution/operators.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <numeric>
#include <optional>
#include <utility>
#include <vector>

#include "global/column_operations.h"

SortOperator::SortOperator(std::unique_ptr<IOperator> child_op,
                           const std::vector<SortKey>& sort_keys)
    : SortOperatorBase(sort_keys), child_op_(std::move(child_op)) {
}

std::optional<ExecBatch> SortOperator::Next() {
    if (done_) {
        return std::nullopt;
    }
    done_ = true;

    std::shared_ptr<const Schema> output_schema;
    std::vector<SortColumn> sort_columns;
    ExecBatch collected;

    std::optional<ExecBatch> optional_batch = child_op_->Next();
    if (!optional_batch.has_value()) {
        return std::nullopt;
    }

    output_schema = optional_batch.value().schema;
    sort_columns = MakeSortColumns(*output_schema);
    collected.schema = output_schema;
    collected.columns = ctp::MakeEmptyColumns(*output_schema);
    collected.row_count = 0;
    
    do {
        ExecBatch& exec_batch = optional_batch.value();
        ctp::AppendColumnarBatch(collected.columns, exec_batch.columns);
        collected.row_count += exec_batch.row_count;
    } while ((optional_batch = child_op_->Next()).has_value());

    std::vector<size_t> row_ids(collected.row_count);
    std::iota(row_ids.begin(), row_ids.end(), size_t{0});
    std::sort(row_ids.begin(), row_ids.end(),
              [&collected, &sort_columns](size_t lhs, size_t rhs) {
                  return SortOperatorBase::RowComesBefore(
                      collected.columns, lhs, collected.columns, rhs,
                      sort_columns);
              });

    return ExecBatch{MakeOutputColumnsByRowIds(collected, row_ids),
                     std::move(output_schema), row_ids.size()};
}
