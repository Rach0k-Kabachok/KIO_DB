#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "transport/compression/column_encoding.h"

std::vector<char> EncodeUnsignedValues(const std::vector<uint64_t>& values);

std::vector<uint64_t> DecodeUnsignedValues(
    const std::vector<char>& payload,
    size_t& offset,
    uint64_t value_count);

uint64_t ZigZagEncode(int64_t value);
int64_t ZigZagDecode(uint64_t value);

class BitPackingEncoding final : public IColumnEncoding {
public:
    kio::Encoding Kind() const override;

    std::vector<char> Encode(
        const ctp::Column& column,
        Schema::Types type) const override;

    ctp::Column Decode(
        const std::vector<char>& payload,
        Schema::Types type,
        uint64_t row_count) const override;
};
