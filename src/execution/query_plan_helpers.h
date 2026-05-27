#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "execution/operators.h"
#include "global/scalar_value.h"
#include "global/schema.h"

namespace exec_plan {

using OperatorPtr = std::unique_ptr<IOperator>;
using ConstraintList = std::shared_ptr<std::vector<MinMaxConstraint>>;

std::vector<std::string> AllColumns(const std::string& db_filename);

OperatorPtr Scan(const std::string& db_filename,
                 const std::vector<std::string>& columns,
                 ConstraintList constraints = nullptr);
OperatorPtr Filter(OperatorPtr child, FilterOperator::RowPredicate predicate);
OperatorPtr Project(OperatorPtr child, std::vector<std::string> columns);
OperatorPtr Compute(
    OperatorPtr child,
    std::vector<ComputeOperator::ComputedColumnSpec> specs);
OperatorPtr Global(OperatorPtr child, std::vector<AggregateSpec> aggregates);
OperatorPtr Group(OperatorPtr child, std::vector<std::string> keys,
                  std::vector<AggregateSpec> aggregates);
OperatorPtr Sort(OperatorPtr child, std::vector<SortKey> keys);
OperatorPtr TopK(OperatorPtr child, std::vector<SortKey> keys, size_t limit);
OperatorPtr Limit(OperatorPtr child, size_t limit, size_t offset = 0);
OperatorPtr OrderedLimit(OperatorPtr child, std::vector<SortKey> keys,
                         size_t limit, size_t offset = 0);

AggregateSpec Count(std::string name = "count");
AggregateSpec Sum(std::string column, std::string name);
AggregateSpec Avg(std::string column, std::string name);
AggregateSpec Min(std::string column, std::string name);
AggregateSpec Max(std::string column, std::string name);
AggregateSpec CountDistinct(std::string column, std::string name);

MinMaxConstraint EqConstraint(std::string column, Schema::Types type,
                              scalar::Value value);
MinMaxConstraint BetweenConstraint(std::string column, Schema::Types type,
                                   scalar::Value lower, scalar::Value upper);
MinMaxConstraint NotEqConstraint(std::string column, Schema::Types type,
                                 scalar::Value value);
ConstraintList Constraints(std::vector<MinMaxConstraint> constraints);

ExecBatch Collect(OperatorPtr op);

}  // namespace exec_plan
