#pragma once

#include <cstddef>
#include <cstring>
#include <string>
#include <vector>

#include "columnar_types.h"
#include "transport/kio/kio_format.h"
#include "schema.h"

namespace kio {

size_t GetColumnPayloadSize(const ctp::Column& column);

size_t GetBatchPayloadSize(const ctp::ColumnarBatch& batch);

std::vector<char> SerializeColumn(const ctp::Column& column,
                                  Schema::Types type);

// Вспомогательные функции перевода типизированного преставления в послежовательность байт
template<typename T>
std::vector<char> SerializeNumericColumn(const std::vector<T>& values) {
    std::vector<char> result(values.size() * sizeof(T));
    std::memcpy(result.data(), values.data(), result.size());
    return result;
}

std::vector<char> SerializeStringColumn(const std::vector<std::string>& strings);

}  // namespace kio
