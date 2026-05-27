#include "execution/predicates.h"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace exec_pred {

Predicate StringNotEmpty(std::string column_name) {
    return [column_name = std::move(column_name),
            column_idx = std::optional<size_t>{}](
               const ExecBatch& batch, size_t row_idx) mutable {
        if (!column_idx.has_value()) {
            column_idx = batch.schema->ColumnIndex(column_name);
        }
        return !Values<std::string>(batch, *column_idx)[row_idx].empty();
    };
}

Predicate Contains(std::string column_name, std::string needle) {
    return [column_name = std::move(column_name), needle = std::move(needle),
            column_idx = std::optional<size_t>{}](
               const ExecBatch& batch, size_t row_idx) mutable {
        if (!column_idx.has_value()) {
            column_idx = batch.schema->ColumnIndex(column_name);
        }
        const std::string& value =
            Values<std::string>(batch, *column_idx)[row_idx];
        return value.find(needle) != std::string::npos;
    };
}

Predicate NotContains(std::string column_name, std::string needle) {
    return [predicate = Contains(std::move(column_name), std::move(needle))](
               const ExecBatch& batch, size_t row_idx) mutable {
        return !predicate(batch, row_idx);
    };
}

Predicate InSmallInt(std::string column_name, int16_t lhs, int16_t rhs) {
    return [column_name = std::move(column_name), lhs, rhs,
            column_idx = std::optional<size_t>{}](
               const ExecBatch& batch, size_t row_idx) mutable {
        if (!column_idx.has_value()) {
            column_idx = batch.schema->ColumnIndex(column_name);
        }
        const int16_t value = Values<int16_t>(batch, *column_idx)[row_idx];
        return value == lhs || value == rhs;
    };
}

Predicate And(std::vector<Predicate> predicates) {
    return [predicates = std::move(predicates)](
               const ExecBatch& batch, size_t row_idx) mutable {
        for (Predicate& predicate : predicates) {
            if (!predicate(batch, row_idx)) {
                return false;
            }
        }
        return true;
    };
}

}  // namespace exec_pred
