#include "transport/compression/delta_length_byte_array_encoding.h"

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "transport/compression/bit_packing_encoding.h"

namespace {

std::vector<char> EncodeStringColumn(
    const std::vector<std::string>& values) {
    std::vector<uint64_t> lengths;
    lengths.reserve(values.size());
    size_t bytes_size = 0;
    for (const std::string& value : values) {
        lengths.push_back(value.size());
        bytes_size += value.size();
    }

    std::vector<char> result = EncodeUnsignedValues(lengths);
    const size_t offset = result.size();
    result.resize(offset + bytes_size);

    size_t write_pos = offset;
    for (const std::string& value : values) {
        if (!value.empty()) {
            std::memcpy(result.data() + write_pos, value.data(), value.size());
            write_pos += value.size();
        }
    }

    return result;
}

ctp::Column DecodeStringColumn(const std::vector<char>& payload,
                               uint64_t row_count) {
    size_t offset = 0;
    std::vector<uint64_t> lengths =
        DecodeUnsignedValues(payload, offset, row_count);

    std::vector<std::string> result;
    result.reserve(row_count);
    for (uint64_t length : lengths) {
        std::string value(length, '\0');
        if (length != 0) {
            std::memcpy(value.data(), payload.data() + offset,
                        length);
        }
        offset += length;
        result.push_back(std::move(value));
    }

    return result;
}

}  // namespace

kio::Encoding DeltaLengthByteArrayEncoding::Kind() const {
    return kio::Encoding::DELTA_LENGTH_BYTE_ARRAY;
}

std::vector<char> DeltaLengthByteArrayEncoding::Encode(
    const ctp::Column& column,
    Schema::Types type) const {
    switch (type) {
    case Schema::TEXT:
    case Schema::VARCHAR:
        return EncodeStringColumn(std::get<std::vector<std::string>>(column));
    case Schema::BIGINT:
    case Schema::INTEGER:
    case Schema::SMALLINT:
    case Schema::CHAR:
    case Schema::TIMESTAMP:
    case Schema::DATE:
        break;
    }

    throw std::invalid_argument(
        "Delta-length byte array encoding supports only strings");
}

ctp::Column DeltaLengthByteArrayEncoding::Decode(
    const std::vector<char>& payload,
    Schema::Types type,
    uint64_t row_count) const {
    switch (type) {
    case Schema::TEXT:
    case Schema::VARCHAR:
        return DecodeStringColumn(payload, row_count);
    case Schema::BIGINT:
    case Schema::INTEGER:
    case Schema::SMALLINT:
    case Schema::CHAR:
    case Schema::TIMESTAMP:
    case Schema::DATE:
        break;
    }

    throw std::invalid_argument(
        "Delta-length byte array encoding supports only strings");
}
