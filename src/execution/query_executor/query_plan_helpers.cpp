#include "execution/query_executor/query_plan_helpers.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "global/column_operations.h"
#include "global/columnar_types.h"
#include "transport/kio/kio_db_reader.h"

namespace exec_plan {

std::vector<std::string> AllColumns(const std::string& db_filename) {
    KioDbReader reader(db_filename);
    const Schema& schema = reader.GetSchema();
    std::vector<std::string> columns;
    columns.reserve(schema.ColumnCount());
    for (size_t idx = 0; idx < schema.ColumnCount(); idx++) {
        columns.push_back(schema.ColumnName(idx));
    }
    return columns;
}

OperatorPtr Scan(const std::string& db_filename,
                 const std::vector<std::string>& columns,
                 ConstraintList constraints) {
    return std::make_unique<TableScanOperator>(
        db_filename, columns, std::move(constraints));
}

OperatorPtr Filter(OperatorPtr child, FilterOperator::RowPredicate predicate) {
    return std::make_unique<FilterOperator>(
        std::move(child), std::move(predicate));
}

OperatorPtr Project(OperatorPtr child, std::vector<std::string> columns) {
    return std::make_unique<ProjectOperator>(std::move(child),
                                             std::move(columns));
}

OperatorPtr Compute(
    OperatorPtr child,
    std::vector<ComputeOperator::ComputedColumnSpec> specs) {
    return std::make_unique<ComputeOperator>(std::move(child), std::move(specs));
}

OperatorPtr Global(OperatorPtr child, std::vector<AggregateSpec> aggregates) {
    return std::make_unique<GlobalAgrOperator>(std::move(child),
                                               std::move(aggregates));
}

OperatorPtr Group(OperatorPtr child, std::vector<std::string> keys,
                  std::vector<AggregateSpec> aggregates) {
    return std::make_unique<GroupAgrOperator>(
        std::move(child), std::move(keys), std::move(aggregates));
}

OperatorPtr Sort(OperatorPtr child, std::vector<SortKey> keys) {
    return std::make_unique<SortOperator>(std::move(child), std::move(keys));
}

OperatorPtr TopK(OperatorPtr child, std::vector<SortKey> keys, size_t limit) {
    return std::make_unique<TopKOperator>(
        std::move(child), std::move(keys), limit);
}

OperatorPtr Limit(OperatorPtr child, size_t limit, size_t offset) {
    return std::make_unique<LimitOperator>(std::move(child), limit, offset);
}

OperatorPtr OrderedLimit(OperatorPtr child, std::vector<SortKey> keys,
                         size_t limit, size_t offset) {
    if (offset == 0) {
        return TopK(std::move(child), std::move(keys), limit);
    }
    return Limit(TopK(std::move(child), std::move(keys), limit + offset),
                 limit, offset);
}

AggregateSpec Count(std::string name) {
    return AggregateSpec{AggregateKind::COUNT, "", std::move(name)};
}

AggregateSpec Sum(std::string column, std::string name) {
    return AggregateSpec{AggregateKind::SUM, std::move(column), std::move(name)};
}

AggregateSpec Avg(std::string column, std::string name) {
    return AggregateSpec{AggregateKind::AVG, std::move(column), std::move(name)};
}

AggregateSpec Min(std::string column, std::string name) {
    return AggregateSpec{AggregateKind::MIN, std::move(column), std::move(name)};
}

AggregateSpec Max(std::string column, std::string name) {
    return AggregateSpec{AggregateKind::MAX, std::move(column), std::move(name)};
}

AggregateSpec CountDistinct(std::string column, std::string name) {
    return AggregateSpec{
        AggregateKind::COUNT_DISTINCT, std::move(column), std::move(name)};
}

MinMaxConstraint EqConstraint(std::string column, Schema::Types type,
                              scalar::Value value) {
    return MinMaxConstraint{
        std::move(column), type, value, value, true, true, false, int64_t{0}};
}

MinMaxConstraint BetweenConstraint(std::string column, Schema::Types type,
                                   scalar::Value lower, scalar::Value upper) {
    return MinMaxConstraint{
        std::move(column), type, lower, upper, true, true, false, int64_t{0}};
}

MinMaxConstraint NotEqConstraint(std::string column, Schema::Types type,
                                 scalar::Value value) {
    return MinMaxConstraint{
        std::move(column), type, std::nullopt, std::nullopt,
        true, true, true, value};
}

ConstraintList Constraints(std::vector<MinMaxConstraint> constraints) {
    return std::make_shared<std::vector<MinMaxConstraint>>(
        std::move(constraints));
}

namespace {
ExecBatch EmptyBatch() {
    return ExecBatch{ctp::ColumnarBatch{},
                     std::make_shared<Schema>(
                         Schema::FromColumns({}, {})),
                     0};
}
}  // namespace

ExecBatch Collect(OperatorPtr op) {
    std::optional<ExecBatch> optional_batch = op->Next();
    if (!optional_batch.has_value()) {
        return EmptyBatch();
    }

    ExecBatch result = std::move(*optional_batch);
    while ((optional_batch = op->Next()).has_value()) {
        ExecBatch& batch = *optional_batch;
        for (size_t row_idx = 0; row_idx < batch.row_count; row_idx++) {
            for (size_t col_idx = 0; col_idx < batch.columns.size(); col_idx++) {
                ctp::AppendColumnValue(result.columns[col_idx],
                                       batch.columns[col_idx],
                                       row_idx);
            }
            result.row_count++;
        }
    }

    return result;
}

}  // namespace exec_plan
