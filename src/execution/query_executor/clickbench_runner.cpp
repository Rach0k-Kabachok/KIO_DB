#include "execution/query_executor/clickbench_queries.h"

#include "execution/query_executor/query_plan_helpers.h"

ExecBatch ExecuteClickBenchQuery(const std::string& db_filename, int query_id) {
    return exec_plan::Collect(MakeClickBenchQuery(db_filename, query_id));
}
