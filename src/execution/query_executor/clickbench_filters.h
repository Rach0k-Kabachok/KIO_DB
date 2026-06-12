#pragma once

#include <string>
#include <vector>

#include "execution/operators/table_scan_op.h"
#include "execution/query_executor/predicates.h"
#include "execution/query_executor/query_plan_helpers.h"

namespace clickbench {

exec_pred::Predicate JulyFilters(std::vector<exec_pred::Predicate> extra = {});
exec_plan::ConstraintList JulyConstraints(
    std::vector<MinMaxConstraint> extra = {});
exec_plan::OperatorPtr QueryWithSearchPhrase(std::string db_filename,
                                             std::vector<std::string> columns);

}  // namespace clickbench
