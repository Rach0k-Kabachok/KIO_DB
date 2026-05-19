#pragma once

#include <string>

#include "execution/operators.h"

ExecBatch ExecuteClickBenchQuery(const std::string& db_filename, int query_id);
