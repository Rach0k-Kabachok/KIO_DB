#pragma once

#include <cstddef>
#include <cstring>
#include <string>
#include <vector>

#include "columnar_types.h"
#include "schema.h"

namespace kio {

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
