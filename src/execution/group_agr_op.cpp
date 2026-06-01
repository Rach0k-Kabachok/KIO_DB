#include "operators.h"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "global/column_operations.h"
#include "global/columnar_types.h"
#include "global/scalar_value.h"
#include "global/schema.h"

namespace {

constexpr size_t kInitialCompactGroupReserve = 4096;

size_t InitialCompactGroupReserve(size_t first_batch_rows) {
    return std::min(first_batch_rows, kInitialCompactGroupReserve);
}

ctp::ColumnarBatch MakeOutputColumns(
    const std::vector<Schema::Types>& output_types, size_t reserve_rows) {
    ctp::ColumnarBatch output_columns;
    output_columns.reserve(output_types.size());
    for (Schema::Types type : output_types) {
        output_columns.push_back(ctp::MakeEmptyColumn(type, reserve_rows));
    }
    return output_columns;
}

uint64_t EncodeGroupValue(const ctp::Column& column,
                          Schema::Types type,
                          size_t row_idx) {
    switch (type) {
    case Schema::BIGINT:
    case Schema::TIMESTAMP:
        return std::bit_cast<uint64_t>(
            std::get<std::vector<int64_t>>(column)[row_idx]);
    case Schema::INTEGER:
    case Schema::DATE:
        return std::bit_cast<uint32_t>(
            std::get<std::vector<int32_t>>(column)[row_idx]);
    case Schema::SMALLINT:
        return std::bit_cast<uint16_t>(
            std::get<std::vector<int16_t>>(column)[row_idx]);
    case Schema::TEXT:
    case Schema::VARCHAR:
        return static_cast<uint64_t>(
            std::hash<std::string_view>()(
                std::get<std::vector<std::string>>(column)[row_idx]));
    case Schema::CHAR:
        return static_cast<unsigned char>(
            std::get<std::vector<char>>(column)[row_idx]);
    case Schema::DOUBLE:
        return static_cast<uint64_t>(
            std::hash<double>()(
                std::get<std::vector<double>>(column)[row_idx]));
    }

    throw std::invalid_argument("Unsupported group column type");
}

void FillEncodedGroupKey(const ExecBatch& batch,
                         const std::vector<size_t>& group_indices,
                         const std::vector<Schema::Types>& group_types,
                         size_t row_idx,
                         std::vector<uint64_t>& key) {
    for (size_t idx = 0; idx < group_indices.size(); idx++) {
        key[idx] = EncodeGroupValue(
            batch.columns[group_indices[idx]], group_types[idx], row_idx);
    }
}

size_t HashEncodedKey(const uint64_t* key, size_t key_size) {
    size_t result = 0;
    for (size_t idx = 0; idx < key_size; idx++) {
        result = scalar::HashCombine(result, std::hash<uint64_t>()(key[idx]));
    }
    return result;
}

bool EncodedKeysEqual(const uint64_t* lhs,
                      const uint64_t* rhs,
                      size_t key_size) {
    for (size_t idx = 0; idx < key_size; idx++) {
        if (lhs[idx] != rhs[idx]) {
            return false;
        }
    }
    return true;
}

bool ColumnValuesEqual(const ctp::Column& lhs,
                       const ctp::Column& rhs,
                       Schema::Types type,
                       size_t lhs_row,
                       size_t rhs_row) {
    switch (type) {
    case Schema::BIGINT:
    case Schema::TIMESTAMP:
        return std::get<std::vector<int64_t>>(lhs)[lhs_row] ==
               std::get<std::vector<int64_t>>(rhs)[rhs_row];
    case Schema::INTEGER:
    case Schema::DATE:
        return std::get<std::vector<int32_t>>(lhs)[lhs_row] ==
               std::get<std::vector<int32_t>>(rhs)[rhs_row];
    case Schema::SMALLINT:
        return std::get<std::vector<int16_t>>(lhs)[lhs_row] ==
               std::get<std::vector<int16_t>>(rhs)[rhs_row];
    case Schema::TEXT:
    case Schema::VARCHAR:
        return std::get<std::vector<std::string>>(lhs)[lhs_row] ==
               std::get<std::vector<std::string>>(rhs)[rhs_row];
    case Schema::CHAR:
        return std::get<std::vector<char>>(lhs)[lhs_row] ==
               std::get<std::vector<char>>(rhs)[rhs_row];
    case Schema::DOUBLE:
        return std::get<std::vector<double>>(lhs)[lhs_row] ==
               std::get<std::vector<double>>(rhs)[rhs_row];
    }

    throw std::invalid_argument("Unsupported group column type");
}

bool StoredGroupKeyEqualsInput(
        const ctp::ColumnarBatch& group_key_columns,
        const ExecBatch& batch,
        const std::vector<size_t>& group_indices,
        const std::vector<Schema::Types>& group_types,
        size_t group_row,
        size_t input_row) {
    for (size_t idx = 0; idx < group_indices.size(); idx++) {
        if (!ColumnValuesEqual(group_key_columns[idx],
                               batch.columns[group_indices[idx]],
                               group_types[idx],
                               group_row,
                               input_row)) {
            return false;
        }
    }
    return true;
}

void AppendGroupKey(ctp::ColumnarBatch& group_key_columns,
                    const ExecBatch& batch,
                    const std::vector<size_t>& group_indices,
                    size_t row_idx) {
    for (size_t idx = 0; idx < group_indices.size(); idx++) {
        ctp::AppendColumnValue(group_key_columns[idx],
                               batch.columns[group_indices[idx]],
                               row_idx);
    }
}

class CompactGroupTable {
public:
    CompactGroupTable(size_t key_size, size_t reserve_groups)
        : key_size_(key_size) {
        Reserve(reserve_groups);
    }

    template <typename EqualsExisting>
    std::pair<size_t, bool> FindOrInsert(
            const std::vector<uint64_t>& key,
            EqualsExisting equals_existing) {
        if (ShouldGrow()) {
            Rehash(slots_.empty() ? kInitialCapacity : slots_.size() * 2);
        }

        const size_t slot_idx = FindSlot(key.data(), equals_existing);
        Slot& slot = slots_[slot_idx];
        if (slot.occupied) {
            return {slot.group_idx, false};
        }

        const size_t group_idx = group_count_++;
        keys_.insert(keys_.end(), key.begin(), key.end());
        slot.group_idx = group_idx;
        slot.occupied = true;
        return {group_idx, true};
    }

    size_t Size() const {
        return group_count_;
    }

private:
    struct Slot {
        size_t group_idx = 0;
        bool occupied = false;
    };

    static constexpr size_t kInitialCapacity = 8;

    static size_t CapacityFor(size_t expected_groups) {
        size_t capacity = kInitialCapacity;
        while (capacity * 3 < expected_groups * 4) {
            capacity *= 2;
        }
        return capacity;
    }

    void Reserve(size_t expected_groups) {
        const size_t capacity = CapacityFor(expected_groups);
        if (capacity > slots_.size()) {
            Rehash(capacity);
        }
        keys_.reserve(expected_groups * key_size_);
    }

    bool ShouldGrow() const {
        return slots_.empty() ||
               (group_count_ + 1) * 4 >= slots_.size() * 3;
    }

    const uint64_t* StoredKey(size_t group_idx) const {
        return keys_.data() + group_idx * key_size_;
    }

    template <typename EqualsExisting>
    size_t FindSlot(const uint64_t* key,
                    EqualsExisting equals_existing) const {
        size_t slot_idx = HashEncodedKey(key, key_size_) & (slots_.size() - 1);
        while (true) {
            const Slot& slot = slots_[slot_idx];
            if (!slot.occupied) {
                return slot_idx;
            }
            if (EncodedKeysEqual(StoredKey(slot.group_idx), key, key_size_) &&
                equals_existing(slot.group_idx)) {
                return slot_idx;
            }
            slot_idx = (slot_idx + 1) & (slots_.size() - 1);
        }
    }

    size_t FindEmptySlot(const uint64_t* key) const {
        size_t slot_idx = HashEncodedKey(key, key_size_) & (slots_.size() - 1);
        while (slots_[slot_idx].occupied) {
            slot_idx = (slot_idx + 1) & (slots_.size() - 1);
        }
        return slot_idx;
    }

    void Rehash(size_t new_capacity) {
        std::vector<Slot> old_slots = std::move(slots_);
        slots_.clear();
        slots_.resize(new_capacity);

        for (const Slot& slot : old_slots) {
            if (!slot.occupied) {
                continue;
            }
            slots_[FindEmptySlot(StoredKey(slot.group_idx))] = slot;
        }
    }

    size_t key_size_;
    size_t group_count_ = 0;
    std::vector<Slot> slots_;
    std::vector<uint64_t> keys_;
};

struct GroupAggregateState {
    AggregateKind kind = AggregateKind::COUNT;
    size_t column_idx = 0;
    Schema::Types input_type = Schema::BIGINT;
    Schema::Types result_type = Schema::BIGINT;
    ctp::Column result;
    std::vector<double> avg_sums;
    std::vector<int64_t> avg_counts;
    std::vector<scalar::DistinctSet> distinct_values;
};

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

class GroupAggregateAccumulator {
public:
    GroupAggregateAccumulator(
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

    void AddGroup(const ExecBatch& batch, size_t row_idx) {
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

    void UpdateGroup(const ExecBatch& batch, size_t group_idx, size_t row_idx) {
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

    void AppendOutputColumns(ctp::ColumnarBatch& output_columns) {
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

private:
    std::vector<GroupAggregateState> states_;
};

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
        InitialCompactGroupReserve(first_batch.row_count);
    CompactGroupTable groups(group_indices.size(), initial_reserve);
    ctp::ColumnarBatch group_key_columns =
        MakeOutputColumns(group_types, initial_reserve);
    GroupAggregateAccumulator aggregate_accumulator(
        aggregates_, aggregate_metadata, initial_reserve);
    std::vector<uint64_t> encoded_key(group_indices.size());

    while (optional_batch.has_value()) {
        ExecBatch& exec_batch = optional_batch.value();

        for (size_t row_idx = 0; row_idx < exec_batch.row_count; row_idx++) {
            FillEncodedGroupKey(
                exec_batch, group_indices, group_types, row_idx, encoded_key);
            auto [group_idx, inserted] = groups.FindOrInsert(
                encoded_key,
                [&group_key_columns, &exec_batch, &group_indices,
                 &group_types, row_idx](size_t existing_group_idx) {
                    return StoredGroupKeyEqualsInput(
                        group_key_columns, exec_batch, group_indices,
                        group_types, existing_group_idx, row_idx);
                });

            if (inserted) {
                AppendGroupKey(group_key_columns, exec_batch,
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
