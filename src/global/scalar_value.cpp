#include "global/scalar_value.h"

#include <functional>
#include <stdexcept>
#include <type_traits>

namespace scalar {

Value GetValue(const ctp::Column& column, size_t row_idx) {
    return std::visit([row_idx](const auto& values) -> Value {
        return values[row_idx];
    }, column);
}

Row MakeRow(const ctp::ColumnarBatch& columns, size_t row_idx) {
    Row row;
    row.reserve(columns.size());

    for (const ctp::Column& column : columns) {
        row.push_back(GetValue(column, row_idx));
    }

    return row;
}

void AppendValue(ctp::Column& column, const Value& value) {
    std::visit([&column](const auto& scalar) {
        using Scalar = std::decay_t<decltype(scalar)>;
        std::get<std::vector<Scalar>>(column).push_back(scalar);
    }, value);
}

int Compare(const Value& lhs, const Value& rhs) {
    return std::visit([&rhs](const auto& lhs_value) {
        using Scalar = std::decay_t<decltype(lhs_value)>;
        const Scalar& rhs_value = std::get<Scalar>(rhs);
        if (lhs_value == rhs_value) {
            return 0;
        }
        return lhs_value < rhs_value ? -1 : 1;
    }, lhs);
}

size_t HashCombine(size_t seed, size_t value) {
    return seed ^ (value + 0x9e3779b9 + (seed << 6) + (seed >> 2));
}

size_t ValueHash::operator()(const Value& value) const {
    return std::visit([](const auto& scalar) {
        using Scalar = std::decay_t<decltype(scalar)>;
        return std::hash<Scalar>()(scalar);
    }, value);
}

size_t RowHash::operator()(const Row& row) const {
    size_t result = 0;
    ValueHash value_hash;
    for (const Value& value : row) {
        result = HashCombine(result, value_hash(value));
    }
    return result;
}

DistinctSet MakeDistinctSet(Schema::Types type) {
    switch (type) {
    case Schema::BIGINT:
    case Schema::TIMESTAMP:
        return std::unordered_set<int64_t>();
    case Schema::INTEGER:
    case Schema::DATE:
        return std::unordered_set<int32_t>();
    case Schema::SMALLINT:
        return std::unordered_set<int16_t>();
    case Schema::DOUBLE:
        return std::unordered_set<double>();
    case Schema::TEXT:
    case Schema::VARCHAR:
        return std::unordered_set<std::string>();
    case Schema::CHAR:
        return std::unordered_set<char>();
    }

    throw std::invalid_argument("Unsupported column type");
}

int64_t DistinctCount(const DistinctSet& values) {
    return std::visit([](const auto& unique_values) {
        return static_cast<int64_t>(unique_values.size());
    }, values);
}

}  // namespace scalar
