#include "execution/operators.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <utility>
#include <variant>

#include "global/columnar_types.h"

namespace {
ctp::Column SliceColumn(const ctp::Column& column, size_t start, size_t count) {
    return std::visit(
        [start, count](const auto& values) -> ctp::Column {
            using Values = std::decay_t<decltype(values)>;
            const auto first = values.begin() + static_cast<std::ptrdiff_t>(start);
            const auto last = first + static_cast<std::ptrdiff_t>(count);
            return Values(first, last);
        },
        column);
}

ctp::ColumnarBatch SliceBatch(const ctp::ColumnarBatch& batch, size_t start,
                              size_t count) {
    ctp::ColumnarBatch result;
    result.reserve(batch.size());
    for (const ctp::Column& column : batch) {
        result.push_back(SliceColumn(column, start, count));
    }
    return result;
}
}  // namespace

LimitOperator::LimitOperator(std::unique_ptr<IOperator> child_op, size_t limit,
                             size_t offset)
    : child_op_(std::move(child_op)), limit_(limit), offset_(offset) {
}

std::optional<ExecBatch> LimitOperator::Next() {
    while (returned_ < limit_) {
        std::optional<ExecBatch> optional_batch = child_op_->Next();
        if (!optional_batch.has_value()) {
            return std::nullopt;
        }

        ExecBatch& batch = *optional_batch;
        const size_t batch_rows = batch.row_count;
        if (batch_rows == 0) {
            continue;
        }

        if (skipped_ + batch_rows <= offset_) {
            skipped_ += batch_rows;
            continue;
        }

        size_t start = 0;
        if (skipped_ < offset_) {
            start = offset_ - skipped_;
            skipped_ = offset_;
        }

        const size_t available = batch_rows - start;
        const size_t remaining = limit_ - returned_;
        const size_t rows_to_return = std::min(available, remaining);
        returned_ += rows_to_return;

        if (start == 0 && rows_to_return == batch_rows) {
            return optional_batch;
        }

        return ExecBatch{
            SliceBatch(batch.columns, start, rows_to_return),
            batch.schema,
            rows_to_return};
    }

    return std::nullopt;
}
