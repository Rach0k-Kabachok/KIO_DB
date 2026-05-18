#pragma once

#include <cstdint>
#include <vector>

#include "global/columnar_types.h"
#include "global/schema.h"
#include "transport/kio/kio_format.h"

struct PreparedColumn {
    std::vector<char> payload;
    kio::Encoding encoding = kio::Encoding::PLAIN;
    kio::Compression compression = kio::Compression::NONE;
    uint64_t compressed_size = 0;
    uint64_t uncompressed_size = 0;
};

class IColumnEncoding {
public:
    virtual kio::Encoding Kind() const = 0;

    virtual std::vector<char> Encode(
        const ctp::Column& column,
        Schema::Types type) const = 0;

    virtual ctp::Column Decode(
        const std::vector<char>& payload,
        Schema::Types type,
        uint64_t row_count) const = 0;

    virtual ~IColumnEncoding() = default;
};

PreparedColumn PrepareColumnForWrite(
    const ctp::Column& column,
    Schema::Types type);

ctp::Column DecodeColumnForRead(
    const std::vector<char>& payload,
    Schema::Types type,
    kio::Encoding encoding,
    kio::Compression compression,
    uint64_t row_count,
    uint64_t uncompressed_size);

const IColumnEncoding& SelectEncodingForType(Schema::Types type);
const IColumnEncoding& GetEncoding(kio::Encoding encoding);
