#include "column_operations.h"

#include <stdexcept>
#include <type_traits>

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
    case Schema::DOUBLE:
        return MakeTypedColumn<double>(reserve_rows);
    }
    throw std::invalid_argument("Unsupported column type");
}

void AppendColumnValue(Column& dst, const Column& src, size_t row_idx) {
    std::visit([&dst, row_idx](const auto& values) {
        using Values = std::decay_t<decltype(values)>;
        std::get<Values>(dst).push_back(values[row_idx]);
    }, src);
}

}  // namespace ctp
