#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_set>
#include <variant>
#include <vector>

#include "global/columnar_types.h"
#include "global/schema.h"

namespace scalar {

using Value = std::variant<
    int64_t,
    int32_t,
    int16_t,
    double,
    std::string,
    char,
    unsigned char>;

using Row = std::vector<Value>;

using DistinctSet = std::variant<
    std::unordered_set<int64_t>,
    std::unordered_set<int32_t>,
    std::unordered_set<int16_t>,
    std::unordered_set<double>,
    std::unordered_set<std::string>,
    std::unordered_set<char>,
    std::unordered_set<unsigned char>>;

Value GetValue(const ctp::Column& column, size_t row_idx);
Row MakeRow(const ctp::ColumnarBatch& columns, size_t row_idx);
void AppendValue(ctp::Column& column, const Value& value);
int Compare(const Value& lhs, const Value& rhs);
size_t HashCombine(size_t seed, size_t value);

struct ValueHash {
    size_t operator()(const Value& value) const;
};

struct RowHash {
    size_t operator()(const Row& row) const;
};

DistinctSet MakeDistinctSet(Schema::Types type);
int64_t DistinctCount(const DistinctSet& values);

}  // namespace scalar
