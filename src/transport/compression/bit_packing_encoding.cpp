#include "transport/compression/bit_packing_encoding.h"

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include "transport/compression/binary_codec.h"

namespace {

uint8_t ComputeBitWidth(uint64_t value) {
    uint8_t width = 0;
    while (value != 0) {
        ++width;
        value >>= 1;
    }
    return width;
}

template <typename T>
std::vector<char> EncodeTypedColumn(const std::vector<T>& values) {
    std::vector<uint64_t> encoded_values;
    encoded_values.reserve(values.size());
    for (T value : values) {
        encoded_values.push_back(
            ZigZagEncode(static_cast<int64_t>(value)));
    }
    return EncodeUnsignedValues(encoded_values);
}

template <typename T>
ctp::Column DecodeTypedColumn(const std::vector<char>& payload,
                              uint64_t row_count) {
    size_t offset = 0;
    std::vector<uint64_t> encoded_values =
        DecodeUnsignedValues(payload, offset, row_count);

    std::vector<T> result;
    result.reserve(row_count);
    for (uint64_t value : encoded_values) {
        result.push_back(static_cast<T>(ZigZagDecode(value)));
    }
    return result;
}

}  // namespace

uint64_t ZigZagEncode(int64_t value) {
    return (static_cast<uint64_t>(value) << 1) ^
           static_cast<uint64_t>(value >> 63);
}

int64_t ZigZagDecode(uint64_t value) {
    return static_cast<int64_t>((value >> 1) ^ (~(value & 1) + 1));
}

std::vector<char> EncodeUnsignedValues(const std::vector<uint64_t>& values) {
    const uint64_t max_value =
        values.empty() ? 0 : *std::max_element(values.begin(), values.end());
    const uint8_t bit_width = ComputeBitWidth(max_value);

    std::vector<char> result;
    bcodec::AppendPod(result, bit_width);
    if (bit_width == 0 || values.empty()) {
        return result;
    }

    const size_t total_bits = values.size() * bit_width;
    const size_t total_bytes = (total_bits + 7) / 8;
    const size_t data_offset = result.size();
    result.resize(data_offset + total_bytes, 0);

    size_t bit_pos = 0;
    for (uint64_t value : values) {
        for (uint8_t bit = 0; bit < bit_width; ++bit) {
            if ((value & (uint64_t{1} << bit)) != 0) {
                result[data_offset + bit_pos / 8] |=
                    static_cast<char>(uint8_t{1} << (bit_pos % 8));
            }
            ++bit_pos;
        }
    }

    return result;
}

std::vector<uint64_t> DecodeUnsignedValues(
    const std::vector<char>& payload,
    size_t& offset,
    uint64_t value_count) {
    const uint8_t bit_width = bcodec::ReadPod<uint8_t>(payload, offset);

    std::vector<uint64_t> result(value_count, 0);
    if (bit_width == 0 || value_count == 0) {
        return result;
    }

    size_t bit_pos = 0;
    for (uint64_t value_idx = 0; value_idx < value_count; ++value_idx) {
        uint64_t value = 0;
        for (uint8_t bit = 0; bit < bit_width; ++bit) {
            const unsigned char byte =
                static_cast<unsigned char>(payload[offset + bit_pos / 8]);
            if ((byte & (uint8_t{1} << (bit_pos % 8))) != 0) {
                value |= uint64_t{1} << bit;
            }
            ++bit_pos;
        }
        result[value_idx] = value;
    }

    offset += (value_count * bit_width + 7) / 8;
    return result;
}

kio::Encoding BitPackingEncoding::Kind() const {
    return kio::Encoding::BIT_PACKING;
}

std::vector<char> BitPackingEncoding::Encode(
    const ctp::Column& column,
    Schema::Types type) const {
    switch (type) {
    case Schema::BIGINT:
    case Schema::TIMESTAMP:
        return EncodeTypedColumn(std::get<std::vector<int64_t>>(column));
    case Schema::INTEGER:
    case Schema::DATE:
        return EncodeTypedColumn(std::get<std::vector<int32_t>>(column));
    case Schema::SMALLINT:
        return EncodeTypedColumn(std::get<std::vector<int16_t>>(column));
    case Schema::CHAR:
        return EncodeTypedColumn(std::get<std::vector<char>>(column));
    case Schema::TEXT:
    case Schema::VARCHAR:
    case Schema::DOUBLE:
        return EncodeTypedColumn(std::get<std::vector<double>>(column));
    }

    throw std::invalid_argument("Bit packing supports only fixed-width types");
}

ctp::Column BitPackingEncoding::Decode(
    const std::vector<char>& payload,
    Schema::Types type,
    uint64_t row_count) const {
    switch (type) {
    case Schema::BIGINT:
    case Schema::TIMESTAMP:
        return DecodeTypedColumn<int64_t>(payload, row_count);
    case Schema::INTEGER:
    case Schema::DATE:
        return DecodeTypedColumn<int32_t>(payload, row_count);
    case Schema::SMALLINT:
        return DecodeTypedColumn<int16_t>(payload, row_count);
    case Schema::CHAR:
        return DecodeTypedColumn<char>(payload, row_count);
    case Schema::TEXT:
    case Schema::VARCHAR:
    case Schema::DOUBLE:
        break;
    }

    throw std::invalid_argument("Bit packing supports only fixed-width types");
}
