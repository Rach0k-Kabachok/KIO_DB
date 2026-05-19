#include "transport/kio/kio_serialization.h"

#include <cstring>
#include <stdexcept>
#include <variant>

namespace kio {

size_t GetColumnPayloadSize(const ctp::Column& column) {
    size_t result = 0;

    std::visit([&result](const auto& values) {
        using Values = std::decay_t<decltype(values)>;
        using Value = typename Values::value_type;

        if constexpr (std::is_same_v<Value, std::string>) {
            result += sizeof(uint64_t) * values.size();
            for (const auto& value : values) {
                result += value.size();
            }
        } else {
            result += sizeof(Value) * values.size();
        }
    }, column);

    return result;
}

size_t GetBatchPayloadSize(const ctp::ColumnarBatch& batch) {
    size_t result = 0;
    for (const auto& column : batch) {
        result += GetColumnPayloadSize(column);
    }
    return result;
}

std::vector<char> SerializeStringColumn(
    const std::vector<std::string>& strings) {
    std::vector<uint64_t> str_sizes;
    str_sizes.reserve(strings.size());
    size_t strings_size = 0;

    for (const auto& str : strings) {
        str_sizes.push_back(str.size());
        strings_size += str.size();
    }

    const size_t sizes_bytes = str_sizes.size() * sizeof(uint64_t);
    std::vector<char> result(sizes_bytes + strings_size);
    std::memcpy(result.data(), str_sizes.data(), sizes_bytes);

    size_t offset = sizes_bytes;
    for (const auto& str : strings) {
        std::memcpy(result.data() + offset, str.data(), str.size());
        offset += str.size();
    }

    return result;
}

std::vector<char> SerializeColumn(
    const ctp::Column& column, Schema::Types type) {
    std::vector<char> payload;

    switch (type) {
    case Schema::BIGINT:
    case Schema::TIMESTAMP:
        payload = SerializeNumericColumn(std::get<std::vector<int64_t>>(column));
        break;
    case Schema::INTEGER:
    case Schema::DATE:
        payload = SerializeNumericColumn(std::get<std::vector<int32_t>>(column));
        break;
    case Schema::SMALLINT:
        payload = SerializeNumericColumn(std::get<std::vector<int16_t>>(column));
        break;
    case Schema::CHAR:
        payload = SerializeNumericColumn(std::get<std::vector<char>>(column));
        break;
    case Schema::TEXT:
    case Schema::VARCHAR: {
        const auto& strings = std::get<std::vector<std::string>>(column);
        payload = SerializeStringColumn(strings);
        break;
    }
    case Schema::DOUBLE:
        payload = SerializeNumericColumn(std::get<std::vector<double>>(column));
        break;
    default:
        throw std::invalid_argument("Unsupported column type");
    }

    return payload;
}

}  // namespace kio
