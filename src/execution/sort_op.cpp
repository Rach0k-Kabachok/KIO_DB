#include "execution/operators.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

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
    std::vector<std::vector<scalar::Row>> sorted_batches;

    std::optional<ExecBatch> optional_batch = child_op_->Next();
    while (optional_batch.has_value()) {
        ExecBatch& batch = optional_batch.value();

        if (!output_schema) {
            output_schema = batch.schema;
            sort_columns = MakeSortColumns(*output_schema);
        }

        std::vector<scalar::Row> rows = MakeRows(batch);
        if (!rows.empty()) {
            std::sort(
                rows.begin(), rows.end(),
                [&sort_columns](const scalar::Row& lhs,
                                const scalar::Row& rhs) {
                    return SortOperatorBase::CompareRows(lhs, rhs,
                                                         sort_columns);
                });
            sorted_batches.push_back(std::move(rows));
        }

        optional_batch = child_op_->Next();
    }

    if (!output_schema) {
        return std::nullopt;
    }

    std::vector<scalar::Row> rows =
        MergeSortedBatches(std::move(sorted_batches), sort_columns);
    return ExecBatch{MakeOutputColumns(rows, output_schema), std::move(output_schema),
                     rows.size()};
}

std::vector<scalar::Row> SortOperator::MergeSortedBatchPair(
    std::vector<scalar::Row>& lhs,
    std::vector<scalar::Row>& rhs,
    const std::vector<SortColumn>& sort_columns) const {
    std::vector<scalar::Row> merged;
    merged.reserve(lhs.size() + rhs.size());

    size_t lhs_idx = 0;
    size_t rhs_idx = 0;

    while (lhs_idx < lhs.size() && rhs_idx < rhs.size()) {
        if (CompareRows(rhs[rhs_idx], lhs[lhs_idx], sort_columns)) {
            merged.push_back(std::move(rhs[rhs_idx]));
            rhs_idx++;
        } else {
            merged.push_back(std::move(lhs[lhs_idx]));
            lhs_idx++;
        }
    }

    while (lhs_idx < lhs.size()) {
        merged.push_back(std::move(lhs[lhs_idx]));
        lhs_idx++;
    }

    while (rhs_idx < rhs.size()) {
        merged.push_back(std::move(rhs[rhs_idx]));
        rhs_idx++;
    }

    return merged;
}

std::vector<scalar::Row> SortOperator::MergeSortedBatches(
    std::vector<std::vector<scalar::Row>> sorted_batches,
    const std::vector<SortColumn>& sort_columns) const {
    if (sorted_batches.empty()) {
        return {};
    }

    while (sorted_batches.size() > 1) {
        std::vector<std::vector<scalar::Row>> merged_batches;
        merged_batches.reserve((sorted_batches.size() + 1) / 2);

        for (size_t idx = 0; idx < sorted_batches.size(); idx += 2) {
            if (idx + 1 == sorted_batches.size()) {
                merged_batches.push_back(std::move(sorted_batches[idx]));
            } else {
                merged_batches.push_back(MergeSortedBatchPair(
                    sorted_batches[idx], sorted_batches[idx + 1],
                    sort_columns));
            }
        }

        sorted_batches = std::move(merged_batches);
    }

    return std::move(sorted_batches.front());
}
