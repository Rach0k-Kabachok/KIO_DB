#pragma once

#include <memory>
#include <string>

#include "execution/operators.h"

std::unique_ptr<IOperator> MakeClickBenchQuery(const std::string& db_filename,
                                               int query_id);

ExecBatch ExecuteClickBenchQuery(const std::string& db_filename, int query_id);
