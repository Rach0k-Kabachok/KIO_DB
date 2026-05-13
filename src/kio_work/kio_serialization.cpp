#include "kio_work/kio_serialization.h"

#include <cstring>
#include <stdexcept>
#include <variant>

namespace kio {

size_t GetColumnPayloadSize(const ctp::Column& column) {
    size_t result = sizeof(ColumnChunkMeta);

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

std::vector<char> SerializeStringColumn(const std::vector<std::string>& strings, uint64_t& total_size) {
    std::vector<char> result;
    
    // Сначала размеры строк
    std::vector<uint64_t> str_sizes;
    str_sizes.reserve(strings.size());
    total_size = 0;
    
    for (const auto& str : strings) {
        str_sizes.push_back(str.size());
        total_size += str.size();
    }
    
    size_t sizes_bytes = str_sizes.size() * sizeof(uint64_t);
    result.resize(sizes_bytes);
    std::memcpy(result.data(), str_sizes.data(), sizes_bytes);
    
    // Потом сами строки
    for (const auto& str : strings) {
        result.insert(result.end(), str.begin(), str.end());
    }
    
    return result;
}

std::pair<ColumnChunkMeta, std::vector<char>> SerializeColumn(const ctp::Column& column, Schema::Types type) {
    std::vector<char> payload;
    uint64_t payload_size = 0;
    
    switch (type) {
    case Schema::BIGINT:
    case Schema::TIMESTAMP:
        payload = SerializeNumericColumn(std::get<std::vector<int64_t>>(column));
        payload_size = payload.size();
        break;
    case Schema::INTEGER:
    case Schema::DATE:
        payload = SerializeNumericColumn(std::get<std::vector<int32_t>>(column));
        payload_size = payload.size();
        break;
    case Schema::SMALLINT:
        payload = SerializeNumericColumn(std::get<std::vector<int16_t>>(column));
        payload_size = payload.size();
        break;
    case Schema::CHAR:
        payload = SerializeNumericColumn(std::get<std::vector<char>>(column));
        payload_size = payload.size();
        break;
    case Schema::TEXT:
    case Schema::VARCHAR: {
        const auto& strings = std::get<std::vector<std::string>>(column);
        uint64_t strings_total = 0;
        payload = SerializeStringColumn(strings, strings_total);
        payload_size = strings_total;
        break;
    }
    default:
        throw std::invalid_argument("Unsupported column type");
    }
    
    ColumnChunkMeta meta;
    meta.size = payload_size;
    return {meta, payload};
}

}  // namespace kio
