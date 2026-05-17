#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
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


class GlobalAgrOperator: public IOperator {
public:
    GlobalAgrOperator(std::unique_ptr<IOperator> child_op,
                      const std::vector<AggregateSpec>& aggregates);

    virtual std::optional<ExecBatch> Next() override;
    virtual ~GlobalAgrOperator() = default;
private:
    std::unique_ptr<IOperator> child_op_;
    std::vector<AggregateSpec> aggregates_;
    bool done_ = false;
};


class GroupAgrOperator: public IOperator {
public:
    GroupAgrOperator(std::unique_ptr<IOperator> child_op,
                     const std::vector<std::string>& group_columns,
                     const std::vector<AggregateSpec>& aggregates);

    virtual std::optional<ExecBatch> Next() override;
    virtual ~GroupAgrOperator() = default;
private:
    std::unique_ptr<IOperator> child_op_;
    std::vector<std::string> group_columns_;
    std::vector<AggregateSpec> aggregates_;
    bool done_ = false;
};


class SortOperator: public IOperator {
public:
    SortOperator(std::unique_ptr<IOperator> child_op,
                 const std::vector<SortKey>& sort_keys);

    virtual std::optional<ExecBatch> Next() override;
    virtual ~SortOperator() = default;
private:
    std::unique_ptr<IOperator> child_op_;
    std::vector<SortKey> sort_keys_;
    bool done_ = false;
};


class TopKOperator: public IOperator {
public:
    TopKOperator(std::unique_ptr<IOperator> child_op,
                 const std::vector<SortKey>& sort_keys, size_t limit);

    virtual std::optional<ExecBatch> Next() override;
    virtual ~TopKOperator() = default;
private:
    std::unique_ptr<IOperator> child_op_;
    std::vector<SortKey> sort_keys_;
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
