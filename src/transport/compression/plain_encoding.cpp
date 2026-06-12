#include "transport/compression/plain_encoding.h"

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "transport/kio/kio_serialization.h"

namespace {

template <typename T>
ctp::Column DecodeFixedColumn(const std::vector<char>& payload,
                              uint64_t row_count) {
    std::vector<T> result(row_count);
    if (!result.empty()) {
        std::memcpy(result.data(), payload.data(), payload.size());
    }
    return result;
}

ctp::Column DecodeStringColumn(const std::vector<char>& payload,
                               uint64_t row_count) {
    std::vector<std::string> result;
    result.reserve(row_count);

    size_t offset = 0;
    std::vector<uint64_t> sizes(row_count);
    const size_t sizes_bytes = sizes.size() * sizeof(uint64_t);
    if (sizes_bytes != 0) {
        std::memcpy(sizes.data(), payload.data(), sizes_bytes);
    }
    offset += sizes_bytes;

    for (uint64_t size : sizes) {
        std::string value(size, '\0');
        if (size != 0) {
            std::memcpy(value.data(), payload.data() + offset,
                        size);
        }
        offset += size;
        result.push_back(std::move(value));
    }

    return result;
}

}  // namespace

kio::Encoding PlainEncoding::Kind() const {
    return kio::Encoding::PLAIN;
}

std::vector<char> PlainEncoding::Encode(
    const ctp::Column& column,
    Schema::Types type) const {
    return kio::SerializeColumn(column, type);
}

ctp::Column PlainEncoding::Decode(
    const std::vector<char>& payload,
    Schema::Types type,
    uint64_t row_count) const {
    switch (type) {
    case Schema::BIGINT:
    case Schema::TIMESTAMP:
        return DecodeFixedColumn<int64_t>(payload, row_count);
    case Schema::INTEGER:
    case Schema::DATE:
        return DecodeFixedColumn<int32_t>(payload, row_count);
    case Schema::SMALLINT:
        return DecodeFixedColumn<int16_t>(payload, row_count);
    case Schema::CHAR:
        return DecodeFixedColumn<char>(payload, row_count);
    case Schema::TEXT:
    case Schema::VARCHAR:
        return DecodeStringColumn(payload, row_count);
    case Schema::DOUBLE:
        return DecodeFixedColumn<double>(payload, row_count);
    }

    throw std::invalid_argument("Unsupported schema type");
}
