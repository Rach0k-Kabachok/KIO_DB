#include "execution/operators/aggregate_ops.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "execution/operators/group/compact_group_table.h"
#include "execution/operators/group/group_aggregate_accumulator.h"
#include "execution/operators/group/group_key_utils.h"
#include "global/columnar_types.h"
#include "global/schema.h"

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
    std::vector<AggregateState> aggregate_metadata = MakeAggregateStates(
        first_batch, aggregate_names, aggregate_types);

    std::vector<std::string> output_names = group_columns_;
    std::vector<Schema::Types> output_types = group_types;
    output_names.insert(output_names.end(),
                        aggregate_names.begin(), aggregate_names.end());
    output_types.insert(output_types.end(),
                        aggregate_types.begin(), aggregate_types.end());

    std::shared_ptr<const Schema> output_schema = std::make_shared<Schema>(
        Schema::FromColumns(output_names, output_types));

    const size_t initial_reserve =
        exec_group::InitialCompactGroupReserve(first_batch.row_count);
    exec_group::CompactGroupTable groups(group_indices.size(), initial_reserve);
    ctp::ColumnarBatch group_key_columns =
        exec_group::MakeOutputColumns(group_types, initial_reserve);
    exec_group::GroupAggregateAccumulator aggregate_accumulator(
        aggregates_, aggregate_metadata, initial_reserve);
    std::vector<uint64_t> encoded_key(group_indices.size());

    while (optional_batch.has_value()) {
        ExecBatch& exec_batch = optional_batch.value();

        for (size_t row_idx = 0; row_idx < exec_batch.row_count; row_idx++) {
            exec_group::FillEncodedGroupKey(
                exec_batch, group_indices, group_types, row_idx, encoded_key);
            auto [group_idx, inserted] = groups.FindOrInsert(
                encoded_key,
                [&group_key_columns, &exec_batch, &group_indices,
                 &group_types, row_idx](size_t existing_group_idx) {
                    return exec_group::StoredGroupKeyEqualsInput(
                        group_key_columns, exec_batch, group_indices,
                        group_types, existing_group_idx, row_idx);
                });

            if (inserted) {
                exec_group::AppendGroupKey(group_key_columns, exec_batch,
                                           group_indices, row_idx);
                aggregate_accumulator.AddGroup(exec_batch, row_idx);
            } else {
                aggregate_accumulator.UpdateGroup(
                    exec_batch, group_idx, row_idx);
            }
        }

        optional_batch = child_op_->Next();
    }

    ctp::ColumnarBatch output_columns = std::move(group_key_columns);
    aggregate_accumulator.AppendOutputColumns(output_columns);

    return ExecBatch{std::move(output_columns), output_schema, groups.Size()};
}
