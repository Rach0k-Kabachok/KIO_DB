#include "operators.h"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <type_traits>
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
    std::unordered_set<std::string>,
    std::unordered_set<char>,
    std::unordered_set<unsigned char>>;

Schema::Types InferAggregateResultType(AggregateKind kind,
                                       Schema::Types input_type) {
    switch (kind) {
    case AggregateKind::COUNT:
    case AggregateKind::SUM:
    case AggregateKind::AVG:
    case AggregateKind::COUNT_DISTINCT:
        return Schema::BIGINT;
    case AggregateKind::MIN:
    case AggregateKind::MAX:
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
void AggregateOperatorBase::ExecGlobalOperation<AggregateKind::COUNT>(
        AggregateState& state, const ExecBatch& exec_batch) {
    std::get<std::vector<int64_t>>(state.result)[0] +=
        static_cast<int64_t>(exec_batch.row_count);
}

template<>
void AggregateOperatorBase::ExecGlobalOperation<AggregateKind::SUM>(
        AggregateState& state, const ExecBatch& exec_batch) {
    std::vector<int64_t>& result_values =
        std::get<std::vector<int64_t>>(state.result);
    const ctp::Column& input = exec_batch.columns[state.column_idx];

    std::visit([&result_values](const auto& values) {
        using Value = typename std::decay_t<decltype(values)>::value_type;
        if constexpr (std::is_arithmetic_v<Value>) {
            for (Value value : values) {
                result_values[0] += value;
            }
        }
    }, input);
}

template<>
void AggregateOperatorBase::ExecGlobalOperation<AggregateKind::AVG>(
        AggregateState& state, const ExecBatch& exec_batch) {
    ExecGlobalOperation<AggregateKind::SUM>(state, exec_batch);
    state.avg_count += static_cast<int64_t>(exec_batch.row_count);
}

template<>
void AggregateOperatorBase::ExecGlobalOperation<AggregateKind::MIN>(
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
void AggregateOperatorBase::ExecGlobalOperation<AggregateKind::MAX>(
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
void AggregateOperatorBase::ExecGlobalOperation<AggregateKind::COUNT_DISTINCT>(
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
void AggregateOperatorBase::ExecGroupOperation<AggregateKind::COUNT>(
        AggregateState& state, const ExecBatch& exec_batch, size_t row_idx) {
    std::get<std::vector<int64_t>>(state.result)[0]++;
}

template<>
void AggregateOperatorBase::ExecGroupOperation<AggregateKind::SUM>(
        AggregateState& state, const ExecBatch& exec_batch, size_t row_idx) {

    std::vector<int64_t>& result_values =
        std::get<std::vector<int64_t>>(state.result);
    const ctp::Column& input = exec_batch.columns[state.column_idx];

    std::visit([&result_values, row_idx](const auto& values) {
        using Value = typename std::decay_t<decltype(values)>::value_type;
        if constexpr (std::is_arithmetic_v<Value>) {
            result_values[0] += values[row_idx];
        }
    }, input);
}

template<>
void AggregateOperatorBase::ExecGroupOperation<AggregateKind::AVG>(
        AggregateState& state, const ExecBatch& exec_batch, size_t row_idx) {
    ExecGroupOperation<AggregateKind::SUM>(state, exec_batch, row_idx);
    state.avg_count++;
}

template<>
void AggregateOperatorBase::ExecGroupOperation<AggregateKind::MIN>(
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
void AggregateOperatorBase::ExecGroupOperation<AggregateKind::MAX>(
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
void AggregateOperatorBase::ExecGroupOperation<AggregateKind::COUNT_DISTINCT>(
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
        if (aggregate.kind != AggregateKind::COUNT) {
            state.column_idx =
                first_batch.schema->ColumnIndex(aggregate.column_name);
            state.input_type = first_batch.schema->ColumnType(state.column_idx);
        }

        state.result_type =
            InferAggregateResultType(aggregate.kind, state.input_type);
        state.result = ctp::MakeEmptyColumn(state.result_type);

        result_names.push_back(aggregate.result_name);
        result_types.push_back(state.result_type);

        if (aggregate.kind == AggregateKind::COUNT ||
            aggregate.kind == AggregateKind::SUM ||
            aggregate.kind == AggregateKind::AVG ||
            aggregate.kind == AggregateKind::COUNT_DISTINCT) {
            std::get<std::vector<int64_t>>(state.result).push_back(0);
        }

        if (aggregate.kind == AggregateKind::COUNT_DISTINCT) {
            state.distinct_values = MakeDistinctSet(state.input_type);
        }

        aggregate_states.push_back(std::move(state));
    }

    return aggregate_states;
}

void AggregateOperatorBase::ApplyAggregateOperation(
        size_t idx, AggregateState& state, const ExecBatch& exec_batch) {
    const AggregateSpec& aggregate = aggregates_[idx];
    
    if (aggregate.kind == AggregateKind::COUNT) {
        ExecGlobalOperation<AggregateKind::COUNT>(state, exec_batch);
    } else if (aggregate.kind == AggregateKind::SUM) {
        ExecGlobalOperation<AggregateKind::SUM>(state, exec_batch);
    } else if (aggregate.kind == AggregateKind::AVG) {
        ExecGlobalOperation<AggregateKind::AVG>(state, exec_batch);
    } else if (aggregate.kind == AggregateKind::MIN) {
        ExecGlobalOperation<AggregateKind::MIN>(state, exec_batch);
    } else if (aggregate.kind == AggregateKind::MAX) {
        ExecGlobalOperation<AggregateKind::MAX>(state, exec_batch);
    } else if (aggregate.kind == AggregateKind::COUNT_DISTINCT) {
        ExecGlobalOperation<AggregateKind::COUNT_DISTINCT>(state, exec_batch);
    } else {
        throw std::invalid_argument("wrong type of operation");
    }
}

void AggregateOperatorBase::ApplyAggregateOperation(
        size_t idx, AggregateState& state, const ExecBatch& exec_batch,
        size_t row_idx) {
    const AggregateSpec& aggregate = aggregates_[idx];

    if (aggregate.kind == AggregateKind::COUNT) {
        ExecGroupOperation<AggregateKind::COUNT>(state, exec_batch, row_idx);
    } else if (aggregate.kind == AggregateKind::SUM) {
        ExecGroupOperation<AggregateKind::SUM>(state, exec_batch, row_idx);
    } else if (aggregate.kind == AggregateKind::AVG) {
        ExecGroupOperation<AggregateKind::AVG>(state, exec_batch, row_idx);
    } else if (aggregate.kind == AggregateKind::MIN) {
        ExecGroupOperation<AggregateKind::MIN>(state, exec_batch, row_idx);
    } else if (aggregate.kind == AggregateKind::MAX) {
        ExecGroupOperation<AggregateKind::MAX>(state, exec_batch, row_idx);
    } else if (aggregate.kind == AggregateKind::COUNT_DISTINCT) {
        ExecGroupOperation<AggregateKind::COUNT_DISTINCT>(
            state, exec_batch, row_idx);
    } else {
        throw std::invalid_argument("wrong type of operation");
    }
}

ctp::ColumnarBatch AggregateOperatorBase::FinalizeAggregation(
        std::vector<AggregateState>& aggregate_states) const {
    for (size_t idx = 0; idx < aggregates_.size(); idx++) {
        if (aggregates_[idx].kind == AggregateKind::AVG &&
            aggregate_states[idx].avg_count != 0) {
            std::get<std::vector<int64_t>>(aggregate_states[idx].result)[0] /=
                aggregate_states[idx].avg_count;
        } else if (aggregates_[idx].kind == AggregateKind::COUNT_DISTINCT) {
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
