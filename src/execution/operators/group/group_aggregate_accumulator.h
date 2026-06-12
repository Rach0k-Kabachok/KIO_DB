#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "execution/operators/aggregate_ops.h"
#include "global/columnar_types.h"
#include "global/scalar_value.h"

namespace exec_group {

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

class GroupAggregateAccumulator {
public:
    GroupAggregateAccumulator(
            const std::vector<AggregateSpec>& aggregates,
            const std::vector<AggregateOperatorBase::AggregateState>& metadata,
            size_t reserve_groups);

    void AddGroup(const ExecBatch& batch, size_t row_idx);
    void UpdateGroup(const ExecBatch& batch, size_t group_idx, size_t row_idx);
    void AppendOutputColumns(ctp::ColumnarBatch& output_columns);

private:
    std::vector<GroupAggregateState> states_;
};

}  // namespace exec_group
