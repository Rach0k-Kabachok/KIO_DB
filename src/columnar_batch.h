#pragma once

#include <cstddef>

#include "columnar_types.h"
#include "schema.h"

namespace ctp {

// Проверить соответствие типа колонки
bool ColumnMatchesType(const Column& column, Schema::Types type);

// Валидировать parsed batch перед преобразованием в columnar
void ValidateParsedBatch(const ParsedBatch& batch, const Schema& schema);

// Валидировать columnar batch согласно схеме
// Проверяет: количество колонок, типы, размеры строк
void ValidateColumnarBatch(const ColumnarBatch& batch, const Schema& schema);

}  // namespace ctp
