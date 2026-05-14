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

// Получить значение из колонки по индексу строки
std::string GetColumnValue(const Column& column, size_t row_idx, Schema::Types type);

// Получить значение как строку без форматирования (для отладки)
std::string GetColumnValueAsString(const Column& column, size_t row_idx, Schema::Types type);

// Применить операцию к каждому элементу колонки
template<typename F>
void VisitColumnElements(const Column& column, F&& visitor) {
    std::visit([&visitor](const auto& values) {
        for (const auto& value : values) {
            visitor(value);
        }
    }, column);
}

}  // namespace ctp
