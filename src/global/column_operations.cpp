#include "column_operations.h"

#include <stdexcept>

namespace ctp {

namespace {
template <typename T>
Column MakeTypedColumn(size_t reserve_rows) {
    std::vector<T> values;
    values.reserve(reserve_rows);
    return values;
}
}

Column MakeEmptyColumn(Schema::Types type, size_t reserve_rows) {
    switch (type) {
    case Schema::BIGINT:
    case Schema::TIMESTAMP:
        return MakeTypedColumn<int64_t>(reserve_rows);
    case Schema::INTEGER:
    case Schema::DATE:
        return MakeTypedColumn<int32_t>(reserve_rows);
    case Schema::SMALLINT:
        return MakeTypedColumn<int16_t>(reserve_rows);
    case Schema::TEXT:
    case Schema::VARCHAR:
        return MakeTypedColumn<std::string>(reserve_rows);
    case Schema::CHAR:
        return MakeTypedColumn<char>(reserve_rows);
    }
    throw std::invalid_argument("Unsupported column type");
}

void AppendColumnValue(Column& dst, const Column& src, size_t row_idx,
                       Schema::Types type) {
    switch (type) {
    case Schema::BIGINT:
    case Schema::TIMESTAMP:
        std::get<std::vector<int64_t>>(dst).push_back(
            std::get<std::vector<int64_t>>(src)[row_idx]);
        break;
    case Schema::INTEGER:
    case Schema::DATE:
        std::get<std::vector<int32_t>>(dst).push_back(
            std::get<std::vector<int32_t>>(src)[row_idx]);
        break;
    case Schema::SMALLINT:
        std::get<std::vector<int16_t>>(dst).push_back(
            std::get<std::vector<int16_t>>(src)[row_idx]);
        break;
    case Schema::TEXT:
    case Schema::VARCHAR:
        std::get<std::vector<std::string>>(dst).push_back(
            std::get<std::vector<std::string>>(src)[row_idx]);
        break;
    case Schema::CHAR:
        std::get<std::vector<char>>(dst).push_back(
            std::get<std::vector<char>>(src)[row_idx]);
        break;
    }
}

}  // namespace ctp
