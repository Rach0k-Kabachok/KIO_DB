#include "execution/operators/group/group_aggregate_accumulator.h"

#include <type_traits>
#include <utility>

#include "global/column_operations.h"

namespace exec_group {
namespace {

void AppendSumInitial(GroupAggregateState& state,
                      const ctp::Column& input,
                      size_t row_idx) {
    std::visit([row_idx](auto& result_values, const auto& values) {
        using ResultValue =
            typename std::decay_t<decltype(result_values)>::value_type;
        using Value = typename std::decay_t<decltype(values)>::value_type;

        if constexpr (std::is_arithmetic_v<ResultValue> &&
                      std::is_arithmetic_v<Value>) {
            result_values.push_back(static_cast<ResultValue>(values[row_idx]));
        }
    }, state.result, input);
}

void UpdateSum(GroupAggregateState& state,
               const ctp::Column& input,
               size_t group_idx,
               size_t row_idx) {
    std::visit([group_idx, row_idx](auto& result_values, const auto& values) {
        using ResultValue =
            typename std::decay_t<decltype(result_values)>::value_type;
        using Value = typename std::decay_t<decltype(values)>::value_type;

        if constexpr (std::is_arithmetic_v<ResultValue> &&
                      std::is_arithmetic_v<Value>) {
            result_values[group_idx] += values[row_idx];
        }
    }, state.result, input);
}

void AppendAvgInitial(GroupAggregateState& state,
                      const ctp::Column& input,
                      size_t row_idx) {
    std::visit([&state, row_idx](const auto& values) {
        using Value = typename std::decay_t<decltype(values)>::value_type;

        if constexpr (std::is_arithmetic_v<Value>) {
            state.avg_sums.push_back(static_cast<double>(values[row_idx]));
            state.avg_counts.push_back(1);
        }
    }, input);
}

void UpdateAvg(GroupAggregateState& state,
               const ctp::Column& input,
               size_t group_idx,
               size_t row_idx) {
    std::visit([&state, group_idx, row_idx](const auto& values) {
        using Value = typename std::decay_t<decltype(values)>::value_type;

        if constexpr (std::is_arithmetic_v<Value>) {
            state.avg_sums[group_idx] += values[row_idx];
            state.avg_counts[group_idx]++;
        }
    }, input);
}

void AppendMinMaxInitial(GroupAggregateState& state,
                         const ctp::Column& input,
                         size_t row_idx) {
    std::visit([row_idx](auto& result_values, const auto& values) {
        using ResultValue =
            typename std::decay_t<decltype(result_values)>::value_type;
        using Value = typename std::decay_t<decltype(values)>::value_type;

        if constexpr (std::is_same_v<ResultValue, Value>) {
            result_values.push_back(values[row_idx]);
        }
    }, state.result, input);
}

void UpdateMinMax(GroupAggregateState& state,
                  const ctp::Column& input,
                  size_t group_idx,
                  size_t row_idx) {
    const bool is_min = state.kind == AggregateKind::MIN;
    std::visit([is_min, group_idx, row_idx](
                   auto& result_values, const auto& values) {
        using ResultValue =
            typename std::decay_t<decltype(result_values)>::value_type;
        using Value = typename std::decay_t<decltype(values)>::value_type;

        if constexpr (std::is_same_v<ResultValue, Value>) {
            const Value& value = values[row_idx];
            if ((is_min && value < result_values[group_idx]) ||
                (!is_min && value > result_values[group_idx])) {
                result_values[group_idx] = value;
            }
        }
    }, state.result, input);
}

void InsertDistinctValue(scalar::DistinctSet& distinct_values,
                         const ctp::Column& input,
                         size_t row_idx) {
    std::visit([row_idx](auto& unique_values, const auto& values) {
        using SetValue =
            typename std::decay_t<decltype(unique_values)>::value_type;
        using Value = typename std::decay_t<decltype(values)>::value_type;

        if constexpr (std::is_same_v<SetValue, Value>) {
            unique_values.insert(values[row_idx]);
        }
    }, distinct_values, input);
}

void AppendDistinctInitial(GroupAggregateState& state,
                           const ctp::Column& input,
                           size_t row_idx) {
    state.distinct_values.push_back(scalar::MakeDistinctSet(state.input_type));
    InsertDistinctValue(state.distinct_values.back(), input, row_idx);
}

void UpdateDistinct(GroupAggregateState& state,
                    const ctp::Column& input,
                    size_t group_idx,
                    size_t row_idx) {
    InsertDistinctValue(state.distinct_values[group_idx], input, row_idx);
}

ctp::Column MakeAvgOutput(GroupAggregateState& state) {
    ctp::Column result = ctp::MakeEmptyColumn(Schema::DOUBLE,
                                             state.avg_sums.size());
    auto& values = std::get<std::vector<double>>(result);
    for (size_t idx = 0; idx < state.avg_sums.size(); idx++) {
        if (state.avg_counts[idx] == 0) {
            values.push_back(0.0);
        } else {
            values.push_back(state.avg_sums[idx] / state.avg_counts[idx]);
        }
    }
    return result;
}

ctp::Column MakeDistinctOutput(GroupAggregateState& state) {
    ctp::Column result = ctp::MakeEmptyColumn(Schema::BIGINT,
                                             state.distinct_values.size());
    auto& values = std::get<std::vector<int64_t>>(result);
    for (const scalar::DistinctSet& distinct_values : state.distinct_values) {
        values.push_back(scalar::DistinctCount(distinct_values));
    }
    return result;
}

}  // namespace

GroupAggregateAccumulator::GroupAggregateAccumulator(
        const std::vector<AggregateSpec>& aggregates,
        const std::vector<AggregateOperatorBase::AggregateState>& metadata,
        size_t reserve_groups) {
    states_.reserve(aggregates.size());
    for (size_t idx = 0; idx < aggregates.size(); idx++) {
        GroupAggregateState state;
        state.kind = aggregates[idx].kind;
        state.column_idx = metadata[idx].column_idx;
        state.input_type = metadata[idx].input_type;
        state.result_type = metadata[idx].result_type;

        switch (state.kind) {
        case AggregateKind::AVG:
            state.avg_sums.reserve(reserve_groups);
            state.avg_counts.reserve(reserve_groups);
            break;
        case AggregateKind::COUNT_DISTINCT:
            state.distinct_values.reserve(reserve_groups);
            break;
        default:
            state.result = ctp::MakeEmptyColumn(
                state.result_type, reserve_groups);
            break;
        }

        states_.push_back(std::move(state));
    }
}

void GroupAggregateAccumulator::AddGroup(const ExecBatch& batch,
                                         size_t row_idx) {
    for (GroupAggregateState& state : states_) {
        switch (state.kind) {
        case AggregateKind::COUNT:
            std::get<std::vector<int64_t>>(state.result).push_back(1);
            break;
        case AggregateKind::SUM:
            AppendSumInitial(
                state, batch.columns[state.column_idx], row_idx);
            break;
        case AggregateKind::AVG:
            AppendAvgInitial(
                state, batch.columns[state.column_idx], row_idx);
            break;
        case AggregateKind::MIN:
        case AggregateKind::MAX:
            AppendMinMaxInitial(
                state, batch.columns[state.column_idx], row_idx);
            break;
        case AggregateKind::COUNT_DISTINCT:
            AppendDistinctInitial(
                state, batch.columns[state.column_idx], row_idx);
            break;
        }
    }
}

void GroupAggregateAccumulator::UpdateGroup(const ExecBatch& batch,
                                            size_t group_idx,
                                            size_t row_idx) {
    for (GroupAggregateState& state : states_) {
        switch (state.kind) {
        case AggregateKind::COUNT:
            std::get<std::vector<int64_t>>(state.result)[group_idx]++;
            break;
        case AggregateKind::SUM:
            UpdateSum(
                state, batch.columns[state.column_idx], group_idx, row_idx);
            break;
        case AggregateKind::AVG:
            UpdateAvg(
                state, batch.columns[state.column_idx], group_idx, row_idx);
            break;
        case AggregateKind::MIN:
        case AggregateKind::MAX:
            UpdateMinMax(
                state, batch.columns[state.column_idx], group_idx, row_idx);
            break;
        case AggregateKind::COUNT_DISTINCT:
            UpdateDistinct(
                state, batch.columns[state.column_idx], group_idx, row_idx);
            break;
        }
    }
}

void GroupAggregateAccumulator::AppendOutputColumns(
        ctp::ColumnarBatch& output_columns) {
    for (GroupAggregateState& state : states_) {
        switch (state.kind) {
        case AggregateKind::AVG:
            output_columns.push_back(MakeAvgOutput(state));
            break;
        case AggregateKind::COUNT_DISTINCT:
            output_columns.push_back(MakeDistinctOutput(state));
            break;
        default:
            output_columns.push_back(std::move(state.result));
            break;
        }
    }
}

}  // namespace exec_group
