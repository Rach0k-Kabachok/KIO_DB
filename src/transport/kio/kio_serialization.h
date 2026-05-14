#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "columnar_types.h"
#include "transport/kio/kio_format.h"
#include "schema.h"

namespace kio {

// Получить размер полезной нагрузки для одной колонки
size_t GetColumnPayloadSize(const ctp::Column& column);

// Получить полный размер пакета с метаданными колонок
size_t GetBatchPayloadSize(const ctp::ColumnarBatch& batch);

// Преобразовать колонку в буфер для записи в файл
// Возвращает пару: метаданные и буфер данных
std::pair<ColumnChunkMeta, std::vector<char>> SerializeColumn(const ctp::Column& column, Schema::Types type);

// Вспомогательные функции для сортировки вывода данных
template<typename T>
std::vector<char> SerializeNumericColumn(const std::vector<T>& values) {
    std::vector<char> result(values.size() * sizeof(T));
    std::memcpy(result.data(), values.data(), result.size());
    return result;
}

std::vector<char> SerializeStringColumn(const std::vector<std::string>& strings, uint64_t& total_size);

}  // namespace kio
