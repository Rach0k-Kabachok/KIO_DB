#include "columnar_batch.h"

#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

#include "column_operations.h"

namespace ctp {
bool ColumnMatchesType(const Column& column, Schema::Types type) {
    switch (type) {
    case Schema::BIGINT:
    case Schema::TIMESTAMP:
        return std::holds_alternative<std::vector<int64_t>>(column);
    case Schema::INTEGER:
    case Schema::DATE:
        return std::holds_alternative<std::vector<int32_t>>(column);
    case Schema::SMALLINT:
        return std::holds_alternative<std::vector<int16_t>>(column);
    case Schema::TEXT:
    case Schema::VARCHAR:
        return std::holds_alternative<std::vector<std::string>>(column);
    case Schema::CHAR:
        return std::holds_alternative<std::vector<char>>(column);
    }

    return false;
}

void ValidateParsedBatch(const ParsedBatch& batch, const Schema& schema) {
    const size_t expected_cols = schema.ColumnCount();

    for (size_t row_idx = 0; row_idx < batch.size(); row_idx++) {
        if (batch[row_idx].size() != expected_cols) {
            throw std::runtime_error(
                "Parsed row " + std::to_string(row_idx) + " has " +
                std::to_string(batch[row_idx].size()) + " columns, expected " +
                std::to_string(expected_cols));
        }
    }
}

void ValidateColumnarBatch(const ColumnarBatch& batch, const Schema& schema) {
    if (batch.empty()) {
        return;
    }

    if (batch.size() != schema.ColumnCount()) {
        throw std::runtime_error("Columnar batch has " +
                                 std::to_string(batch.size()) +
                                 " columns, expected " +
                                 std::to_string(schema.ColumnCount()));
    }

    const size_t row_num = GetColumnRowCount(batch[0]);
    for (size_t col_idx = 0; col_idx < batch.size(); col_idx++) {
        if (!ColumnMatchesType(batch[col_idx], schema.ColumnType(col_idx))) {
            throw std::runtime_error("Column " + std::to_string(col_idx) +
                                     " type does not match schema");
        }

        const size_t col_size = GetColumnRowCount(batch[col_idx]);
        if (col_size != row_num) {
            throw std::runtime_error("Column " + std::to_string(col_idx) +
                                     " has " + std::to_string(col_size) +
                                     " rows, expected " +
                                     std::to_string(row_num));
        }
    }
}
}  // namespace ctp
