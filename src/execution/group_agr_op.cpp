#include "operators.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "global/column_operations.h"
#include "global/columnar_types.h"
#include "global/scalar_value.h"
#include "global/schema.h"

namespace {

scalar::Row MakeGroupKey(const ExecBatch& batch,
                         const std::vector<size_t>& group_indices,
                         size_t row_idx) {
    scalar::Row key;
    key.reserve(group_indices.size());
    for (size_t column_idx : group_indices) {
        key.push_back(scalar::GetValue(batch.columns[column_idx], row_idx));
    }
    return key;
}

}  // namespace

GroupAgrOperator::GroupAgrOperator(
        std::unique_ptr<IOperator> child_op,
        const std::vector<std::string>& group_columns,
        const std::vector<AggregateSpec>& aggregates) :
        AggregateOperatorBase(aggregates),
        child_op_(std::move(child_op)),
        group_columns_(group_columns) {
}

std::optional<ExecBatch> GroupAgrOperator::Next() {
    if (done_) {
        return std::nullopt;
    }
    done_ = true;

    std::optional<ExecBatch> optional_batch = child_op_->Next();
    if (!optional_batch.has_value()) {
        return std::nullopt;
    }

    const ExecBatch& first_batch = optional_batch.value();

    std::vector<size_t> group_indices;
    std::vector<Schema::Types> group_types;
    group_indices.reserve(group_columns_.size());
    group_types.reserve(group_columns_.size());

    for (const std::string& column_name : group_columns_) {
        size_t column_idx = first_batch.schema->ColumnIndex(column_name);
        group_indices.push_back(column_idx);
        group_types.push_back(first_batch.schema->ColumnType(column_idx));
    }

    std::vector<std::string> aggregate_names;
    std::vector<Schema::Types> aggregate_types;
    std::vector<AggregateState> initial_states = MakeAggregateStates(
        first_batch, aggregate_names, aggregate_types);

    std::vector<std::string> output_names = group_columns_;
    std::vector<Schema::Types> output_types = group_types;
    output_names.insert(output_names.end(),
                        aggregate_names.begin(), aggregate_names.end());
    output_types.insert(output_types.end(),
                        aggregate_types.begin(), aggregate_types.end());

    std::shared_ptr<const Schema> output_schema = std::make_shared<Schema>(
        Schema::FromColumns(output_names, output_types));

    std::unordered_map<scalar::Row, std::vector<AggregateState>,
                       scalar::RowHash>
        groups;

    while (optional_batch.has_value()) {
        ExecBatch& exec_batch = optional_batch.value();

        for (size_t row_idx = 0; row_idx < exec_batch.row_count; row_idx++) {
            scalar::Row key =
                MakeGroupKey(exec_batch, group_indices, row_idx);
            auto [it, inserted] = groups.try_emplace(
                std::move(key), initial_states);

            std::vector<AggregateState>& aggregate_states = it->second;
            for (size_t idx = 0; idx < aggregates_.size(); idx++) {
                ApplyAggregateOperation(
                    idx, aggregate_states[idx], exec_batch, row_idx);
            }
        }

        optional_batch = child_op_->Next();
    }

    ctp::ColumnarBatch output_columns;
    output_columns.reserve(output_types.size());
    for (Schema::Types type : output_types) {
        output_columns.push_back(ctp::MakeEmptyColumn(type, groups.size()));
    }

    for (auto& [key, aggregate_states] : groups) {
        for (size_t idx = 0; idx < key.size(); idx++) {
            scalar::AppendValue(output_columns[idx], key[idx]);
        }

        ctp::ColumnarBatch aggregate_results =
            FinalizeAggregation(aggregate_states);
        for (size_t idx = 0; idx < aggregate_results.size(); idx++) {
            ctp::AppendColumnValue(
                output_columns[group_columns_.size() + idx],
                aggregate_results[idx],
                0);
        }
    }

    return ExecBatch{std::move(output_columns), output_schema, groups.size()};
}
