#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "execution/operators/compute_op.h"

namespace clickbench {

int32_t Date(std::string_view value);

ComputeOperator::ComputedColumnSpec StringLength(std::string column,
                                                 std::string result);
ComputeOperator::ComputedColumnSpec Int32Minus(std::string column,
                                               int32_t value,
                                               std::string result);
ComputeOperator::ComputedColumnSpec SmallIntPlus(std::string column,
                                                 int64_t value,
                                                 std::string result);
ComputeOperator::ComputedColumnSpec MinuteColumn(std::string column,
                                                 std::string result);
ComputeOperator::ComputedColumnSpec DateTruncMinute(std::string column,
                                                    std::string result);
ComputeOperator::ComputedColumnSpec DomainColumn(std::string column,
                                                 std::string result);
ComputeOperator::ComputedColumnSpec ConstantOne();
ComputeOperator::ComputedColumnSpec AggregatedSumPlus(std::string sum_column,
                                                      std::string count_column,
                                                      int64_t multiplier,
                                                      std::string result);
ComputeOperator::ComputedColumnSpec CaseSource();
ComputeOperator::ComputedColumnSpec UrlDestination();

}  // namespace clickbench
