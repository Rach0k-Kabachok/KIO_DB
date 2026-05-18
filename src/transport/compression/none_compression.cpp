#include "transport/compression/none_compression.h"

kio::Compression NoneCompression::Kind() const {
    return kio::Compression::NONE;
}

std::vector<char> NoneCompression::Compress(
    const std::vector<char>& payload) const {
    return payload;
}

std::vector<char> NoneCompression::Decompress(
    const std::vector<char>& payload,
    uint64_t uncompressed_size) const {
    return payload;
}
