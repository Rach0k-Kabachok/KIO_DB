#include "transport/compression/column_encoding.h"

#include <stdexcept>
#include <utility>

#include "transport/compression/bit_packing_encoding.h"
#include "transport/compression/compression_codec.h"
#include "transport/compression/delta_encoding.h"
#include "transport/compression/delta_length_byte_array_encoding.h"
#include "transport/compression/dictionary_encoding.h"
#include "transport/compression/plain_encoding.h"
#include "transport/compression/rle_encoding.h"

const IColumnEncoding& SelectEncodingForType(Schema::Types type) {
    static const PlainEncoding plain_encoding;
    static const RleEncoding rle_encoding;
    static const DictionaryEncoding dictionary_encoding;
    static const DeltaEncoding delta_encoding;

    switch (type) {
    case Schema::BIGINT:
    case Schema::INTEGER:
    case Schema::SMALLINT:
    case Schema::TIMESTAMP:
    case Schema::DATE:
        return delta_encoding;
    case Schema::TEXT:
    case Schema::VARCHAR:
        return dictionary_encoding;
    case Schema::CHAR:
        return rle_encoding;
    }

    return plain_encoding;
}

const IColumnEncoding& GetEncoding(kio::Encoding encoding) {
    static const PlainEncoding plain_encoding;
    static const RleEncoding rle_encoding;
    static const DictionaryEncoding dictionary_encoding;
    static const BitPackingEncoding bit_packing_encoding;
    static const DeltaEncoding delta_encoding;
    static const DeltaLengthByteArrayEncoding delta_length_encoding;

    switch (encoding) {
    case kio::Encoding::PLAIN:
        return plain_encoding;
    case kio::Encoding::RLE:
        return rle_encoding;
    case kio::Encoding::DICTIONARY:
        return dictionary_encoding;
    case kio::Encoding::BIT_PACKING:
        return bit_packing_encoding;
    case kio::Encoding::DELTA:
        return delta_encoding;
    case kio::Encoding::DELTA_LENGTH_BYTE_ARRAY:
        return delta_length_encoding;
    }

    throw std::invalid_argument("Unsupported column encoding");
}

PreparedColumn PrepareColumnForWrite(
    const ctp::Column& column,
    Schema::Types type) {
    const IColumnEncoding& encoding = SelectEncodingForType(type);
    std::vector<char> encoded_payload = encoding.Encode(column, type);

    const ICompressionCodec& compression =
        GetCompressionCodec(kio::Compression::NONE);
    std::vector<char> compressed_payload =
        compression.Compress(encoded_payload);

    PreparedColumn prepared;
    prepared.payload = std::move(compressed_payload);
    prepared.encoding = encoding.Kind();
    prepared.compression = compression.Kind();
    prepared.compressed_size = prepared.payload.size();
    prepared.uncompressed_size = encoded_payload.size();
    return prepared;
}

ctp::Column DecodeColumnForRead(
    const std::vector<char>& payload,
    Schema::Types type,
    kio::Encoding encoding,
    kio::Compression compression,
    uint64_t row_count,
    uint64_t uncompressed_size) {
    const ICompressionCodec& compression_codec =
        GetCompressionCodec(compression);
    std::vector<char> encoded_payload =
        compression_codec.Decompress(payload, uncompressed_size);

    const IColumnEncoding& column_encoding = GetEncoding(encoding);
    return column_encoding.Decode(encoded_payload, type, row_count);
}
