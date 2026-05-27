#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "global/columnar_types.h"
#include "global/scalar_value.h"
#include "global/schema.h"
#include "transport/kio/kio_db_reader.h"


struct ExecBatch {
    ctp::ColumnarBatch columns;
    std::shared_ptr<const Schema> schema;
    size_t row_count = 0;
};

class IOperator {
public:
    IOperator() = default;
    virtual std::optional<ExecBatch> Next() = 0;
    virtual ~IOperator() = default;
};

struct MinMaxConstraint {
    std::string column_name;
    Schema::Types type = Schema::BIGINT;
    std::optional<scalar::Value> lower;
    std::optional<scalar::Value> upper;
    bool lower_inclusive = true;
    bool upper_inclusive = true;
    bool not_equal = false;
    scalar::Value not_equal_value = int64_t{0};
};

class TableScanOperator: public IOperator {
public:
    TableScanOperator(const std::string& db_filename,
                      const std::vector<std::string>& column_names);
    TableScanOperator(const std::string& db_filename,
                      const std::vector<std::string>& column_names,
                      std::shared_ptr<std::vector<MinMaxConstraint>> constraints);

    virtual std::optional<ExecBatch> Next() override;
    virtual ~TableScanOperator() = default;
private:
    std::shared_ptr<const Schema> output_schema_;
    KioDbReader reader_;
    std::vector<size_t> column_indices_;
    std::shared_ptr<std::vector<MinMaxConstraint>> constraints_;
};

class ComputeOperator: public IOperator {
public:
    using RowComputer = std::function<scalar::Value(
        const ExecBatch& batch, size_t row_idx)>;

    struct ComputedColumnSpec {
        std::string name;
        Schema::Types type = Schema::BIGINT;
        RowComputer compute;
    };

    ComputeOperator(std::unique_ptr<IOperator> child_op,
                    std::vector<ComputedColumnSpec> computed_columns);

    virtual std::optional<ExecBatch> Next() override;
    virtual ~ComputeOperator() = default;
private:
    std::unique_ptr<IOperator> child_op_;
    std::vector<ComputedColumnSpec> computed_columns_;
    std::shared_ptr<const Schema> output_schema_;
};


class FilterOperator: public IOperator {
public:
    using RowPredicate = std::function<bool(const ExecBatch& batch, size_t row_idx)>;

    FilterOperator(std::unique_ptr<IOperator> child_op,
                   RowPredicate predicate);

    virtual std::optional<ExecBatch> Next() override;
    virtual ~FilterOperator() = default;
private:
    std::unique_ptr<IOperator> child_op_;
    RowPredicate predicate_;
};


class ProjectOperator: public IOperator {
public:
    ProjectOperator(std::unique_ptr<IOperator> child_op,
                    std::vector<std::string> column_names);

    virtual std::optional<ExecBatch> Next() override;
    virtual ~ProjectOperator() = default;
private:
    std::unique_ptr<IOperator> child_op_;
    std::vector<std::string> column_names_;
    std::vector<size_t> column_indices_;
    std::shared_ptr<const Schema> output_schema_;
};


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
        size_t column_idx = 0;
        Schema::Types input_type = Schema::BIGINT;
        Schema::Types result_type = Schema::BIGINT;
        ctp::Column result;
        int64_t avg_count = 0;
        double avg_sum = 0.0;
        scalar::DistinctSet distinct_values;
    };
protected:
    explicit AggregateOperatorBase(const std::vector<AggregateSpec>& aggregates);
    virtual ~AggregateOperatorBase() = default;

    template<AggregateKind TYPE>
    void ExecGlobalOperation(AggregateState& state,
                             const ExecBatch& exec_batch);

    template<AggregateKind TYPE>
    void ExecGroupOperation(AggregateState& state,
                            const ExecBatch& exec_batch,
                            size_t row_idx);

    std::vector<AggregateState> MakeAggregateStates(
        const ExecBatch& first_batch,
        std::vector<std::string>& result_names,
        std::vector<Schema::Types>& result_types) const;

    void ApplyAggregateOperation(size_t idx, AggregateState& state,
                                 const ExecBatch& exec_batch);
    void ApplyAggregateOperation(size_t idx, AggregateState& state,
                                 const ExecBatch& exec_batch,
                                 size_t row_idx);

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

    virtual std::optional<ExecBatch> Next() override;
    virtual ~GlobalAgrOperator() = default;
private:
    std::unique_ptr<IOperator> child_op_;
    bool done_ = false;
};

class GroupAgrOperator: public IOperator, protected AggregateOperatorBase {
public:
    GroupAgrOperator(std::unique_ptr<IOperator> child_op,
                     const std::vector<std::string>& group_columns,
                     const std::vector<AggregateSpec>& aggregates);

    virtual std::optional<ExecBatch> Next() override;
    virtual ~GroupAgrOperator() = default;
private:
    std::unique_ptr<IOperator> child_op_;
    std::vector<std::string> group_columns_;
    bool done_ = false;
};


class SortOperatorBase {
public:
    enum class SortOrder {
        ASC,
        DESC
    };

    struct SortKey {
        std::string column_name;
        SortOrder order = SortOrder::ASC;
    };

protected:
    explicit SortOperatorBase(const std::vector<SortKey>& sort_keys);
    virtual ~SortOperatorBase() = default;

    struct SortColumn {
        size_t column_idx = 0;
        SortOrder order = SortOrder::ASC;
    };

    std::vector<SortColumn> MakeSortColumns(const Schema& schema) const;

    static bool RowComesBefore(
        const ctp::ColumnarBatch& lhs_columns, size_t lhs_row,
        const ctp::ColumnarBatch& rhs_columns, size_t rhs_row,
        const std::vector<SortColumn>& sort_columns);
    static ctp::ColumnarBatch MakeOutputColumnsByRowIds(
        const ExecBatch& batch, const std::vector<size_t>& row_ids);

    std::vector<SortKey> sort_keys_;
};

using SortOrder = SortOperatorBase::SortOrder;
using SortKey = SortOperatorBase::SortKey;

class SortOperator: public IOperator, protected SortOperatorBase {
public:
    SortOperator(std::unique_ptr<IOperator> child_op,
                 const std::vector<SortKey>& sort_keys);

    virtual std::optional<ExecBatch> Next() override;
    virtual ~SortOperator() = default;
private:
    std::unique_ptr<IOperator> child_op_;
    bool done_ = false;
};


class TopKOperator: public IOperator, protected SortOperatorBase {
public:
    TopKOperator(std::unique_ptr<IOperator> child_op,
                 const std::vector<SortKey>& sort_keys, size_t limit);

    virtual std::optional<ExecBatch> Next() override;
    virtual ~TopKOperator() = default;
private:
    std::unique_ptr<IOperator> child_op_;
    size_t limit_;

    bool done_ = false;
};


class LimitOperator: public IOperator {
public:
    LimitOperator(std::unique_ptr<IOperator> child_op, size_t limit,
                  size_t offset = 0);

    virtual std::optional<ExecBatch> Next() override;
    virtual ~LimitOperator() = default;
private:
    std::unique_ptr<IOperator> child_op_;
    size_t limit_;
    size_t offset_ = 0;
    size_t skipped_ = 0;
    size_t returned_ = 0;
};
