#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>
#include <variant>
#include <vector>

#include "global/columnar_types.h"
#include "global/schema.h"
#include "transport/kio/kio_db_reader.h"

struct ExecBatch {
    ctp::ColumnarBatch columns;
    std::shared_ptr<const Schema> schema;
    size_t row_count = 0;
};

enum class AggregateKind {
    COUNT,
    SUM,
    AVG,
    MIN,
    MAX,
    COUNT_DISTINCT
};

enum class SortOrder {
    ASC,
    DESC
};

struct AggregateSpec {
    AggregateKind kind;
    std::string column_name;
    std::string result_name;
    Schema::Types result_type = Schema::BIGINT;
};

struct SortKey {
    std::string column_name;
    SortOrder order = SortOrder::ASC;
};

using VarType = std::variant<
    size_t,
    int64_t,
    int32_t,
    int16_t,
    std::string,
    char,
    unsigned char>;

using VarVector = std::vector<VarType>;

using RowPredicate = std::function<bool(const ExecBatch& batch, size_t row_idx)>;


class IOperator {
public:
    IOperator() = default;
    virtual std::optional<ExecBatch> Next() = 0;
    virtual ~IOperator() = default;
};


class TableScanOperator: public IOperator {
public:
    TableScanOperator(const std::string& db_filename,
                      const std::vector<std::string>& column_names);

    virtual std::optional<ExecBatch> Next() override;
    virtual ~TableScanOperator() = default;
private:
    std::shared_ptr<const Schema> output_schema_;
    KioDbReader reader_;
    std::vector<size_t> column_indices_;
};


class FilterOperator: public IOperator {
public:
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
protected:
    explicit AggregateOperatorBase(const std::vector<AggregateSpec>& aggregates);
    virtual ~AggregateOperatorBase() = default;

    using DistinctSet = std::variant<
        std::unordered_set<int64_t>,
        std::unordered_set<int32_t>,
        std::unordered_set<int16_t>,
        std::unordered_set<std::string>,
        std::unordered_set<char>,
        std::unordered_set<unsigned char>>;

    struct AggregateState {
        size_t column_idx = 0;
        Schema::Types input_type = Schema::BIGINT;
        Schema::Types result_type = Schema::BIGINT;
        ctp::Column result;
        int64_t avg_count = 0;
        DistinctSet distinct_values;
    };

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
protected:
    explicit SortOperatorBase(const std::vector<SortKey>& sort_keys);
    virtual ~SortOperatorBase() = default;

    struct SortColumn {
        size_t column_idx = 0;
        SortOrder order = SortOrder::ASC;
    };

    std::vector<SortColumn> MakeSortColumns(const Schema& schema) const;

    static bool CompareRows(const VarVector& lhs, const VarVector& rhs,
                            const std::vector<SortColumn>& sort_columns);
    static VarVector MakeRow(const ExecBatch& batch, size_t row_idx);
    static std::vector<VarVector> MakeRows(const ExecBatch& batch);
    static ctp::ColumnarBatch MakeOutputColumns(
        const std::vector<VarVector>& rows,
        const std::shared_ptr<const Schema>& schema);

    std::vector<SortKey> sort_keys_;
};


class SortOperator: public IOperator, protected SortOperatorBase {
public:
    SortOperator(std::unique_ptr<IOperator> child_op,
                 const std::vector<SortKey>& sort_keys);

    virtual std::optional<ExecBatch> Next() override;
    virtual ~SortOperator() = default;
private:
    std::vector<VarVector> MergeSortedBatchPair(
        std::vector<VarVector>& lhs,
        std::vector<VarVector>& rhs,
        const std::vector<SortColumn>& sort_columns) const;
    std::vector<VarVector> MergeSortedBatches(
        std::vector<std::vector<VarVector>> sorted_batches,
        const std::vector<SortColumn>& sort_columns) const;

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
