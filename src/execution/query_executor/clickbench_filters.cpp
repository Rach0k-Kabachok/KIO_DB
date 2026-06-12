#include "execution/query_executor/clickbench_filters.h"

#include <cstdint>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "execution/query_executor/clickbench_expressions.h"

namespace clickbench {

using namespace exec_plan;
using namespace exec_pred;

Predicate JulyFilters(std::vector<Predicate> extra) {
    std::vector<Predicate> predicates{
        EqualTo<int32_t>("CounterID", 62),
        GreaterEqual<int32_t>("EventDate", Date("2013-07-01")),
        LessEqual<int32_t>("EventDate", Date("2013-07-31")),
        EqualTo<int16_t>("IsRefresh", 0)};
    predicates.insert(predicates.end(),
                      std::make_move_iterator(extra.begin()),
                      std::make_move_iterator(extra.end()));
    return And(std::move(predicates));
}

ConstraintList JulyConstraints(std::vector<MinMaxConstraint> extra) {
    std::vector<MinMaxConstraint> constraints{
        EqConstraint("CounterID", Schema::INTEGER, int32_t{62}),
        BetweenConstraint("EventDate", Schema::DATE,
                          Date("2013-07-01"), Date("2013-07-31")),
        EqConstraint("IsRefresh", Schema::SMALLINT, int16_t{0})};
    constraints.insert(constraints.end(),
                       std::make_move_iterator(extra.begin()),
                       std::make_move_iterator(extra.end()));
    return Constraints(std::move(constraints));
}

OperatorPtr QueryWithSearchPhrase(std::string db_filename,
                                  std::vector<std::string> columns) {
    return Filter(Scan(db_filename, columns), StringNotEmpty("SearchPhrase"));
}

}  // namespace clickbench
