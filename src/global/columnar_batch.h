#pragma once


#include "columnar_types.h"
#include "schema.h"

namespace ctp {

// Проверить соответствие типа колонки
bool ColumnMatchesType(const Column& column, Schema::Types type);

// Валидировать пакет строк перед преобразованием в колоночный формат
void ValidateParsedBatch(const ParsedBatch& batch, const Schema& schema);

// Валидировать колоночный пакет согласно схеме
// Проверяет: количество колонок, типы, размеры строк
void ValidateColumnarBatch(const ColumnarBatch& batch, const Schema& schema);

}  // namespace ctp
