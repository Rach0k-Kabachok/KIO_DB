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

ColumnarBatch MakeEmptyColumns(const Schema& schema, size_t reserve_rows) {
    ColumnarBatch columns;
    columns.reserve(schema.ColumnCount());
    for (size_t col_idx = 0; col_idx < schema.ColumnCount(); col_idx++) {
        columns.push_back(MakeEmptyColumn(schema.ColumnType(col_idx),
                                          reserve_rows));
    }
    return columns;
}

void AppendColumnValue(Column& dst, const Column& src, size_t row_idx) {
    std::visit([&dst, row_idx](const auto& values) {
        using Values = std::decay_t<decltype(values)>;
        std::get<Values>(dst).push_back(values[row_idx]);
    }, src);
}

void AppendColumn(Column& dst, const Column& src) {
    std::visit([](auto& dst_values, const auto& src_values) {
        using DstValues = std::decay_t<decltype(dst_values)>;
        using SrcValues = std::decay_t<decltype(src_values)>;
        if constexpr (std::is_same_v<DstValues, SrcValues>) {
            dst_values.insert(dst_values.end(),
                              src_values.begin(), src_values.end());
        }
    }, dst, src);
}

void AppendColumnarBatch(ColumnarBatch& dst, const ColumnarBatch& src) {
    for (size_t col_idx = 0; col_idx < src.size(); col_idx++) {
        AppendColumn(dst[col_idx], src[col_idx]);
    }
}

Column GatherColumnRows(const Column& src,
                        const std::vector<size_t>& row_indices) {
    return std::visit([&row_indices](const auto& values) -> Column {
        using Values = std::decay_t<decltype(values)>;
        Values result;
        result.reserve(row_indices.size());
        for (size_t row_idx : row_indices) {
            result.push_back(values[row_idx]);
        }
        return result;
    }, src);
}

}  // namespace ctp
