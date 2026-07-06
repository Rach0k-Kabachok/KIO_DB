#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "execution/operator.h"
#include "global/columnar_types.h"
#include "global/scalar_value.h"
#include "global/schema.h"

class AggregateOperatorBase {
public:
    enum class AggregateKind {
        COUNT,
        SUM,
        AVG,
        MIN,
        MAX,
        COUNT_DISTINCT
    };

    struct AggregateSpec {
        AggregateKind kind;
        std::string column_name;
        std::string result_name;
        Schema::Types result_type = Schema::BIGINT;
    };

    struct AggregateState {
        AggregateState();
        AggregateState(const AggregateState& other);
        AggregateState& operator=(const AggregateState& other);
        AggregateState(AggregateState&&) noexcept;
        AggregateState& operator=(AggregateState&&) noexcept;
        ~AggregateState();

        size_t column_idx = 0;
        Schema::Types input_type = Schema::BIGINT;
        Schema::Types result_type = Schema::BIGINT;
        ctp::Column result;
        int64_t avg_count = 0;
        double avg_sum = 0.0;
        std::unique_ptr<scalar::DistinctSet> distinct_values;
    };
protected:
    explicit AggregateOperatorBase(const std::vector<AggregateSpec>& aggregates);
    virtual ~AggregateOperatorBase() = default;

    template<AggregateKind TYPE>
    void ExecGlobalOperation(AggregateState& state,
                             const ExecBatch& exec_batch);

    std::vector<AggregateState> MakeAggregateStates(
        const ExecBatch& first_batch,
        std::vector<std::string>& result_names,
        std::vector<Schema::Types>& result_types) const;

    void ApplyAggregateOperation(size_t idx, AggregateState& state,
                                 const ExecBatch& exec_batch);

    ctp::ColumnarBatch FinalizeAggregation(
        std::vector<AggregateState>& aggregate_states) const;

    std::vector<AggregateSpec> aggregates_;
};

using AggregateKind = AggregateOperatorBase::AggregateKind;
using AggregateSpec = AggregateOperatorBase::AggregateSpec;

class GlobalAgrOperator: public IOperator, protected AggregateOperatorBase {
public:
    GlobalAgrOperator(std::unique_ptr<IOperator> child_op,
                      const std::vector<AggregateSpec>& aggregates);

    std::optional<ExecBatch> Next() override;
    ~GlobalAgrOperator() override = default;
private:
    std::unique_ptr<IOperator> child_op_;
    bool done_ = false;
};

class GroupAgrOperator: public IOperator, protected AggregateOperatorBase {
public:
    GroupAgrOperator(std::unique_ptr<IOperator> child_op,
                     const std::vector<std::string>& group_columns,
                     const std::vector<AggregateSpec>& aggregates);

    std::optional<ExecBatch> Next() override;
    ~GroupAgrOperator() override = default;
private:
    std::unique_ptr<IOperator> child_op_;
    std::vector<std::string> group_columns_;
    bool done_ = false;
};
