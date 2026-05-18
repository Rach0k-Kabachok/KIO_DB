#pragma once

#include "transport/compression/column_encoding.h"

class RleEncoding final : public IColumnEncoding {
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
