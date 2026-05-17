#include "operators.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "global/columnar_types.h"
#include "global/schema.h"

GlobalAgrOperator::GlobalAgrOperator(
        std::unique_ptr<IOperator> child_op,
        const std::vector<AggregateSpec>& aggregates) :
        AggregateOperatorBase(aggregates),
        child_op_(std::move(child_op)) {
}

std::optional<ExecBatch> GlobalAgrOperator::Next() {
    if (done_) {
        return std::nullopt;
    }
    done_ = true;

    std::optional<ExecBatch> optional_batch = child_op_->Next();
    if (!optional_batch.has_value()) {
        return std::nullopt;
    }

    std::vector<std::string> result_names;
    std::vector<Schema::Types> result_types;
    const ExecBatch& first_batch = optional_batch.value();
    std::vector<AggregateState> aggregate_states = MakeAggregateStates(
        first_batch, result_names, result_types);

    std::shared_ptr<const Schema> output_schema = std::make_shared<Schema>(
        Schema::FromColumns(result_names, result_types));

    while (optional_batch.has_value()) {
        ExecBatch& exec_batch = optional_batch.value();

        for (size_t idx = 0; idx < aggregates_.size(); idx++) {
            ApplyAggregateOperation(idx, aggregate_states[idx], exec_batch);
        }

        optional_batch = child_op_->Next();
    }

    ctp::ColumnarBatch aggregate_results =
        FinalizeAggregation(aggregate_states);

    return ExecBatch{std::move(aggregate_results), output_schema, 1};
}
