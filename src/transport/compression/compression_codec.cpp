#include "transport/compression/compression_codec.h"

#include <stdexcept>

#include "transport/compression/none_compression.h"

const ICompressionCodec& GetCompressionCodec(kio::Compression compression) {
    static const NoneCompression none_compression;

    switch (compression) {
    case kio::Compression::NONE:
        return none_compression;
    }

    throw std::invalid_argument("Unsupported compression codec");
}
