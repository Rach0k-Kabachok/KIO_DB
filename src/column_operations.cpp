#include "column_operations.h"

#include <stdexcept>

namespace ctp {

std::string GetColumnValue(const Column& column, size_t row_idx, Schema::Types type) {
    switch (type) {
    case Schema::BIGINT:
    case Schema::TIMESTAMP:
        return std::to_string(std::get<std::vector<int64_t>>(column)[row_idx]);
    case Schema::INTEGER:
    case Schema::DATE:
        return std::to_string(std::get<std::vector<int32_t>>(column)[row_idx]);
    case Schema::SMALLINT:
        return std::to_string(std::get<std::vector<int16_t>>(column)[row_idx]);
    case Schema::TEXT:
    case Schema::VARCHAR:
        return std::get<std::vector<std::string>>(column)[row_idx];
    case Schema::CHAR:
        return std::string(1, std::get<std::vector<char>>(column)[row_idx]);
    }
    throw std::invalid_argument("Unsupported column type");
}

std::string GetColumnValueAsString(const Column& column, size_t row_idx, Schema::Types type) {
    return GetColumnValue(column, row_idx, type);
}

}  // namespace ctp
