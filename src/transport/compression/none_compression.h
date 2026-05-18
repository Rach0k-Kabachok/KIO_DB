#pragma once

#include "transport/compression/compression_codec.h"

class NoneCompression final : public ICompressionCodec {
public:
    kio::Compression Kind() const override;

    std::vector<char> Compress(
        const std::vector<char>& payload) const override;

    std::vector<char> Decompress(
        const std::vector<char>& payload,
        uint64_t uncompressed_size) const override;
};
