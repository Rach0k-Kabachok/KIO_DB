#pragma once

#include <string>
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

// Специализация для строк
template<>
inline const std::vector<std::string>& GetColumnData<std::string>(const Column& column) {
    return std::get<std::vector<std::string>>(column);
}

Column MakeEmptyColumn(Schema::Types type, size_t reserve_rows = 0);

void AppendColumnValue(Column& dst, const Column& src, size_t row_idx,
                       Schema::Types type);

}  // namespace ctp
