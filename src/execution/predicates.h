#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "execution/operators.h"
#include "global/column_operations.h"

namespace exec_pred {

using Predicate = FilterOperator::RowPredicate;

template <typename T>
const std::vector<T>& Values(const ExecBatch& batch, size_t column_idx) {
    return ctp::GetColumnData<T>(batch.columns[column_idx]);
}

template <typename T, typename Comparator>
Predicate CompareColumn(std::string column_name, T value,
                        Comparator comparator) {
    return [column_name = std::move(column_name), value,
            comparator = std::move(comparator),
            column_idx = std::optional<size_t>{}](
               const ExecBatch& batch, size_t row_idx) mutable {
        if (!column_idx.has_value()) {
            column_idx = batch.schema->ColumnIndex(column_name);
        }
        return comparator(Values<T>(batch, *column_idx)[row_idx], value);
    };
}

template <typename T>
Predicate EqualTo(std::string column_name, T value) {
    return CompareColumn(
        std::move(column_name), value, std::equal_to<T>{});
}

template <typename T>
Predicate NotEqualTo(std::string column_name, T value) {
    return CompareColumn(
        std::move(column_name), value, std::not_equal_to<T>{});
}

template <typename T>
Predicate GreaterEqual(std::string column_name, T value) {
    return CompareColumn(
        std::move(column_name), value, std::greater_equal<T>{});
}

template <typename T>
Predicate LessEqual(std::string column_name, T value) {
    return CompareColumn(
        std::move(column_name), value, std::less_equal<T>{});
}

Predicate StringNotEmpty(std::string column_name);
Predicate Contains(std::string column_name, std::string needle);
Predicate NotContains(std::string column_name, std::string needle);
Predicate InSmallInt(std::string column_name, int16_t lhs, int16_t rhs);
Predicate And(std::vector<Predicate> predicates);

}  // namespace exec_pred
