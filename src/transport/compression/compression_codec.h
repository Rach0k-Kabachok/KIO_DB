#pragma once

#include <cstdint>
#include <vector>

#include "transport/kio/kio_format.h"

class ICompressionCodec {
public:
    virtual kio::Compression Kind() const = 0;

    virtual std::vector<char> Compress(
        const std::vector<char>& payload) const = 0;

    virtual std::vector<char> Decompress(
        const std::vector<char>& payload,
        uint64_t uncompressed_size) const = 0;

    virtual ~ICompressionCodec() = default;
};

const ICompressionCodec& GetCompressionCodec(kio::Compression compression);
