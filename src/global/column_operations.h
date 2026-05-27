#pragma once

#include <vector>
#include <variant>

#include "columnar_types.h"
#include "schema.h"

namespace ctp {

// Получить количество элементов в колонке
inline size_t GetColumnRowCount(const Column& column) {
    return std::visit([](const auto& values) { return values.size(); }, column);
}

// Шаблонная функция для безопасного извлечения данных из Column
template<typename T>
const std::vector<T>& GetColumnData(const Column& column) {
    return std::get<std::vector<T>>(column);
}

Column MakeEmptyColumn(Schema::Types type, size_t reserve_rows = 0);

ColumnarBatch MakeEmptyColumns(const Schema& schema, size_t reserve_rows = 0);

void AppendColumnValue(Column& dst, const Column& src, size_t row_idx);

void AppendColumn(Column& dst, const Column& src);

void AppendColumnarBatch(ColumnarBatch& dst, const ColumnarBatch& src);

Column GatherColumnRows(const Column& src,
                        const std::vector<size_t>& row_indices);

}  // namespace ctp
