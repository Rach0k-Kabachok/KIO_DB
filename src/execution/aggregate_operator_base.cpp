#include "operators.h"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <vector>

#include "global/column_operations.h"
#include "global/columnar_types.h"
#include "global/schema.h"

namespace {

using DistinctSet = std::variant<
    std::unordered_set<int64_t>,
    std::unordered_set<int32_t>,
    std::unordered_set<int16_t>,
    std::unordered_set<double>,
    std::unordered_set<std::string>,
    std::unordered_set<char>,
    std::unordered_set<unsigned char>>;

Schema::Types InferAggregateResultType(AggregateOperatorBase::AggregateKind kind,
                                       Schema::Types input_type) {
    switch (kind) {
    case AggregateOperatorBase::AggregateKind::COUNT:
    case AggregateOperatorBase::AggregateKind::COUNT_DISTINCT:
        return Schema::BIGINT;
    case AggregateOperatorBase::AggregateKind::SUM:
        return input_type == Schema::DOUBLE ? Schema::DOUBLE : Schema::BIGINT;
    case AggregateOperatorBase::AggregateKind::AVG:
        return Schema::DOUBLE;
    case AggregateOperatorBase::AggregateKind::MIN:
    case AggregateOperatorBase::AggregateKind::MAX:
        return input_type;
    }
    throw std::invalid_argument("wrong type of operation");
}

DistinctSet MakeDistinctSet(Schema::Types type) {
    switch (type) {
    case Schema::BIGINT:
    case Schema::TIMESTAMP:
        return std::unordered_set<int64_t>();
    case Schema::INTEGER:
    case Schema::DATE:
        return std::unordered_set<int32_t>();
    case Schema::SMALLINT:
        return std::unordered_set<int16_t>();
    case Schema::DOUBLE:
        return std::unordered_set<double>();
    case Schema::TEXT:
    case Schema::VARCHAR:
        return std::unordered_set<std::string>();
    case Schema::CHAR:
        return std::unordered_set<char>();
    }
    throw std::invalid_argument("Unsupported column type");
}

int64_t DistinctCount(const DistinctSet& result) {
    return std::visit([](const auto& unique_values) {
        return static_cast<int64_t>(unique_values.size());
    }, result);
}

}  // namespace

AggregateOperatorBase::AggregateOperatorBase(
        const std::vector<AggregateSpec>& aggregates) :
        aggregates_(aggregates) {
}

template<>
void AggregateOperatorBase::ExecGlobalOperation<AggregateOperatorBase::AggregateKind::COUNT>(
        AggregateState& state, const ExecBatch& exec_batch) {
    std::get<std::vector<int64_t>>(state.result)[0] +=
        static_cast<int64_t>(exec_batch.row_count);
}

template<>
void AggregateOperatorBase::ExecGlobalOperation<AggregateOperatorBase::AggregateKind::SUM>(
        AggregateState& state, const ExecBatch& exec_batch) {
    const ctp::Column& input = exec_batch.columns[state.column_idx];

    std::visit([](auto& result_values, const auto& values) {
        using ResultValue =
            typename std::decay_t<decltype(result_values)>::value_type;
        using Value = typename std::decay_t<decltype(values)>::value_type;
        if constexpr (std::is_arithmetic_v<ResultValue> &&
                      std::is_arithmetic_v<Value>) {
            for (Value value : values) {
                result_values[0] += value;
            }
        }
    }, state.result, input);
}

template<>
void AggregateOperatorBase::ExecGlobalOperation<AggregateOperatorBase::AggregateKind::AVG>(
        AggregateState& state, const ExecBatch& exec_batch) {
    const ctp::Column& input = exec_batch.columns[state.column_idx];

    std::visit([&state](const auto& values) {
        using Value = typename std::decay_t<decltype(values)>::value_type;
        if constexpr (std::is_arithmetic_v<Value>) {
            for (Value value : values) {
                state.avg_sum += value;
            }
        }
    }, input);
    state.avg_count += exec_batch.row_count;
}

template<>
void AggregateOperatorBase::ExecGlobalOperation<AggregateOperatorBase::AggregateKind::MIN>(
        AggregateState& state, const ExecBatch& exec_batch) {
    const ctp::Column& input = exec_batch.columns[state.column_idx];

    std::visit([](auto& result_values, const auto& values) {
        using ResultValue =
            typename std::decay_t<decltype(result_values)>::value_type;
        using Value = typename std::decay_t<decltype(values)>::value_type;

        if constexpr (std::is_same_v<ResultValue, Value>) {
            for (const Value& value : values) {
                if (result_values.empty()) {
                    result_values.push_back(value);
                } else if (value < result_values[0]) {
                    result_values[0] = value;
                }
            }
        }
    }, state.result, input);
}

template<>
void AggregateOperatorBase::ExecGlobalOperation<AggregateOperatorBase::AggregateKind::MAX>(
        AggregateState& state, const ExecBatch& exec_batch) {
    const ctp::Column& input = exec_batch.columns[state.column_idx];

    std::visit([](auto& result_values, const auto& values) {
        using ResultValue =
            typename std::decay_t<decltype(result_values)>::value_type;
        using Value = typename std::decay_t<decltype(values)>::value_type;

        if constexpr (std::is_same_v<ResultValue, Value>) {
            for (const Value& value : values) {
                if (result_values.empty()) {
                    result_values.push_back(value);
                } else if (value > result_values[0]) {
                    result_values[0] = value;
                }
            }
        }
    }, state.result, input);
}

template<>
void AggregateOperatorBase::ExecGlobalOperation<AggregateOperatorBase::AggregateKind::COUNT_DISTINCT>(
        AggregateState& state, const ExecBatch& exec_batch) {
    const ctp::Column& input = exec_batch.columns[state.column_idx];

    std::visit([](auto& unique_values, const auto& values) {
        using SetValue =
            typename std::decay_t<decltype(unique_values)>::value_type;
        using Value = typename std::decay_t<decltype(values)>::value_type;

        if constexpr (std::is_same_v<SetValue, Value>) {
            unique_values.insert(values.begin(), values.end());
        }
    }, state.distinct_values, input);
}


template<>
void AggregateOperatorBase::ExecGroupOperation<AggregateOperatorBase::AggregateKind::COUNT>(
        AggregateState& state, const ExecBatch& exec_batch, size_t row_idx) {
    std::get<std::vector<int64_t>>(state.result)[0]++;
}

template<>
void AggregateOperatorBase::ExecGroupOperation<AggregateOperatorBase::AggregateKind::SUM>(
        AggregateState& state, const ExecBatch& exec_batch, size_t row_idx) {

    const ctp::Column& input = exec_batch.columns[state.column_idx];

    std::visit([row_idx](auto& result_values, const auto& values) {
        using ResultValue =
            typename std::decay_t<decltype(result_values)>::value_type;
        using Value = typename std::decay_t<decltype(values)>::value_type;
        if constexpr (std::is_arithmetic_v<ResultValue> &&
                      std::is_arithmetic_v<Value>) {
            result_values[0] += values[row_idx];
        }
    }, state.result, input);
}

template<>
void AggregateOperatorBase::ExecGroupOperation<AggregateOperatorBase::AggregateKind::AVG>(
        AggregateState& state, const ExecBatch& exec_batch, size_t row_idx) {
    const ctp::Column& input = exec_batch.columns[state.column_idx];

    std::visit([&state, row_idx](const auto& values) {
        using Value = typename std::decay_t<decltype(values)>::value_type;
        if constexpr (std::is_arithmetic_v<Value>) {
            state.avg_sum += values[row_idx];
        }
    }, input);
    state.avg_count++;
}

template<>
void AggregateOperatorBase::ExecGroupOperation<AggregateOperatorBase::AggregateKind::MIN>(
        AggregateState& state, const ExecBatch& exec_batch, size_t row_idx) {
    const ctp::Column& input = exec_batch.columns[state.column_idx];

    std::visit([row_idx](auto& result_values, const auto& values) {
        using ResultValue =
            typename std::decay_t<decltype(result_values)>::value_type;
        using Value = typename std::decay_t<decltype(values)>::value_type;

        if constexpr (std::is_same_v<ResultValue, Value>) {
            const Value& value = values[row_idx];
            if (result_values.empty()) {
                result_values.push_back(value);
            } else if (value < result_values[0]) {
                result_values[0] = value;
            }
        }
    }, state.result, input);
}

template<>
void AggregateOperatorBase::ExecGroupOperation<AggregateOperatorBase::AggregateKind::MAX>(
        AggregateState& state, const ExecBatch& exec_batch, size_t row_idx) {
    const ctp::Column& input = exec_batch.columns[state.column_idx];

    std::visit([row_idx](auto& result_values, const auto& values) {
        using ResultValue =
            typename std::decay_t<decltype(result_values)>::value_type;
        using Value = typename std::decay_t<decltype(values)>::value_type;

        if constexpr (std::is_same_v<ResultValue, Value>) {
            const Value& value = values[row_idx];
            if (result_values.empty()) {
                result_values.push_back(value);
            } else if (value > result_values[0]) {
                result_values[0] = value;
            }
        }
    }, state.result, input);
}

template<>
void AggregateOperatorBase::ExecGroupOperation<AggregateOperatorBase::AggregateKind::COUNT_DISTINCT>(
        AggregateState& state, const ExecBatch& exec_batch, size_t row_idx) {
    const ctp::Column& input = exec_batch.columns[state.column_idx];

    std::visit([row_idx](auto& unique_values, const auto& values) {
        using SetValue =
            typename std::decay_t<decltype(unique_values)>::value_type;
        using Value = typename std::decay_t<decltype(values)>::value_type;

        if constexpr (std::is_same_v<SetValue, Value>) {
            unique_values.insert(values[row_idx]);
        }
    }, state.distinct_values, input);
}

std::vector<AggregateOperatorBase::AggregateState>
AggregateOperatorBase::MakeAggregateStates(
        const ExecBatch& first_batch,
        std::vector<std::string>& result_names,
        std::vector<Schema::Types>& result_types) const {
    std::vector<AggregateState> aggregate_states;
    aggregate_states.reserve(aggregates_.size());
    result_names.reserve(aggregates_.size());
    result_types.reserve(aggregates_.size());

    for (const AggregateSpec& aggregate : aggregates_) {
        AggregateState state;
        if (aggregate.kind != AggregateOperatorBase::AggregateKind::COUNT) {
            state.column_idx =
                first_batch.schema->ColumnIndex(aggregate.column_name);
            state.input_type = first_batch.schema->ColumnType(state.column_idx);
        }

        state.result_type =
            InferAggregateResultType(aggregate.kind, state.input_type);
        state.result = ctp::MakeEmptyColumn(state.result_type);

        result_names.push_back(aggregate.result_name);
        result_types.push_back(state.result_type);

        if (state.result_type == Schema::BIGINT &&
            (aggregate.kind == AggregateOperatorBase::AggregateKind::COUNT ||
             aggregate.kind == AggregateOperatorBase::AggregateKind::SUM ||
             aggregate.kind == AggregateOperatorBase::AggregateKind::COUNT_DISTINCT)) {
            std::get<std::vector<int64_t>>(state.result).push_back(0);
        } else if (state.result_type == Schema::DOUBLE &&
                   (aggregate.kind == AggregateOperatorBase::AggregateKind::SUM ||
                    aggregate.kind == AggregateOperatorBase::AggregateKind::AVG)) {
            std::get<std::vector<double>>(state.result).push_back(0.0);
        }

        if (aggregate.kind == AggregateOperatorBase::AggregateKind::COUNT_DISTINCT) {
            state.distinct_values = MakeDistinctSet(state.input_type);
        }

        aggregate_states.push_back(std::move(state));
    }

    return aggregate_states;
}

void AggregateOperatorBase::ApplyAggregateOperation(
        size_t idx, AggregateState& state, const ExecBatch& exec_batch) {
    const AggregateSpec& aggregate = aggregates_[idx];
    
    if (aggregate.kind == AggregateOperatorBase::AggregateKind::COUNT) {
        ExecGlobalOperation<AggregateOperatorBase::AggregateKind::COUNT>(state, exec_batch);
    } else if (aggregate.kind == AggregateOperatorBase::AggregateKind::SUM) {
        ExecGlobalOperation<AggregateOperatorBase::AggregateKind::SUM>(state, exec_batch);
    } else if (aggregate.kind == AggregateOperatorBase::AggregateKind::AVG) {
        ExecGlobalOperation<AggregateOperatorBase::AggregateKind::AVG>(state, exec_batch);
    } else if (aggregate.kind == AggregateOperatorBase::AggregateKind::MIN) {
        ExecGlobalOperation<AggregateOperatorBase::AggregateKind::MIN>(state, exec_batch);
    } else if (aggregate.kind == AggregateOperatorBase::AggregateKind::MAX) {
        ExecGlobalOperation<AggregateOperatorBase::AggregateKind::MAX>(state, exec_batch);
    } else if (aggregate.kind == AggregateOperatorBase::AggregateKind::COUNT_DISTINCT) {
        ExecGlobalOperation<AggregateOperatorBase::AggregateKind::COUNT_DISTINCT>(state, exec_batch);
    } else {
        throw std::invalid_argument("wrong type of operation");
    }
}

void AggregateOperatorBase::ApplyAggregateOperation(
        size_t idx, AggregateState& state, const ExecBatch& exec_batch,
        size_t row_idx) {
    const AggregateSpec& aggregate = aggregates_[idx];

    if (aggregate.kind == AggregateOperatorBase::AggregateKind::COUNT) {
        ExecGroupOperation<AggregateOperatorBase::AggregateKind::COUNT>(state, exec_batch, row_idx);
    } else if (aggregate.kind == AggregateOperatorBase::AggregateKind::SUM) {
        ExecGroupOperation<AggregateOperatorBase::AggregateKind::SUM>(state, exec_batch, row_idx);
    } else if (aggregate.kind == AggregateOperatorBase::AggregateKind::AVG) {
        ExecGroupOperation<AggregateOperatorBase::AggregateKind::AVG>(state, exec_batch, row_idx);
    } else if (aggregate.kind == AggregateOperatorBase::AggregateKind::MIN) {
        ExecGroupOperation<AggregateOperatorBase::AggregateKind::MIN>(state, exec_batch, row_idx);
    } else if (aggregate.kind == AggregateOperatorBase::AggregateKind::MAX) {
        ExecGroupOperation<AggregateOperatorBase::AggregateKind::MAX>(state, exec_batch, row_idx);
    } else if (aggregate.kind == AggregateOperatorBase::AggregateKind::COUNT_DISTINCT) {
        ExecGroupOperation<AggregateOperatorBase::AggregateKind::COUNT_DISTINCT>(
            state, exec_batch, row_idx);
    } else {
        throw std::invalid_argument("wrong type of operation");
    }
}

ctp::ColumnarBatch AggregateOperatorBase::FinalizeAggregation(
        std::vector<AggregateState>& aggregate_states) const {
    for (size_t idx = 0; idx < aggregates_.size(); idx++) {
        if (aggregates_[idx].kind == AggregateOperatorBase::AggregateKind::AVG &&
            aggregate_states[idx].avg_count != 0) {
            std::get<std::vector<double>>(aggregate_states[idx].result)[0] =
                aggregate_states[idx].avg_sum / aggregate_states[idx].avg_count;
        } else if (aggregates_[idx].kind == AggregateOperatorBase::AggregateKind::COUNT_DISTINCT) {
            std::get<std::vector<int64_t>>(aggregate_states[idx].result)[0] =
                DistinctCount(aggregate_states[idx].distinct_values);
        }
    }

    ctp::ColumnarBatch aggregate_results;
    aggregate_results.reserve(aggregate_states.size());
    for (AggregateState& state : aggregate_states) {
        aggregate_results.push_back(std::move(state.result));
    }

    return aggregate_results;
}
