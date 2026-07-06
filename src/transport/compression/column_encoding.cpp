#include "transport/compression/column_encoding.h"

#include <stdexcept>
#include <utility>

#include "transport/compression/bit_packing_encoding.h"
#include "transport/compression/delta_encoding.h"
#include "transport/compression/delta_length_byte_array_encoding.h"
#include "transport/compression/dictionary_encoding.h"
#include "transport/compression/plain_encoding.h"
#include "transport/compression/rle_encoding.h"

namespace { // придумать, как сделать полиморфизм покрасивее
const PlainEncoding kPlainEncoding;
const RleEncoding kRleEncoding;
const DictionaryEncoding kDictionaryEncoding;
const BitPackingEncoding kBitPackingEncoding;
const DeltaEncoding kDeltaEncoding;
const DeltaLengthByteArrayEncoding kDeltaLengthEncoding;
}  // namespace

const IColumnEncoding& GetEncoding(kio::Encoding encoding) {
    switch (encoding) {
    case kio::Encoding::PLAIN:
        return kPlainEncoding;
    case kio::Encoding::RLE:
        return kRleEncoding;
    case kio::Encoding::DICTIONARY:
        return kDictionaryEncoding;
    case kio::Encoding::BIT_PACKING:
        return kBitPackingEncoding;
    case kio::Encoding::DELTA:
        return kDeltaEncoding;
    case kio::Encoding::DELTA_LENGTH_BYTE_ARRAY:
        return kDeltaLengthEncoding;
    }

    throw std::invalid_argument("Unsupported column encoding");
}

const IColumnEncoding& SelectEncodingForType(Schema::Types type) {
    switch (type) {
    case Schema::BIGINT:
    case Schema::INTEGER:
    case Schema::SMALLINT:
    case Schema::TIMESTAMP:
    case Schema::DATE:
        return GetEncoding(kio::Encoding::DELTA);
    case Schema::TEXT:
    case Schema::VARCHAR:
        return GetEncoding(kio::Encoding::DICTIONARY);
    case Schema::CHAR:
        return GetEncoding(kio::Encoding::RLE);
    case Schema::DOUBLE:
        return GetEncoding(kio::Encoding::PLAIN);
    }

    return GetEncoding(kio::Encoding::PLAIN);
}

PreparedColumn PrepareColumnForWrite(
    const ctp::Column& column,
    Schema::Types type) {
    const IColumnEncoding& encoding = SelectEncodingForType(type);
    std::vector<char> encoded_payload = encoding.Encode(column, type);

    PreparedColumn prepared;
    prepared.payload = std::move(encoded_payload);
    prepared.encoding = encoding.Kind();
    prepared.compression = kio::Compression::NONE;
    prepared.compressed_size = prepared.payload.size();
    prepared.uncompressed_size = prepared.payload.size();
    return prepared;
}

ctp::Column DecodeColumnForRead(
    const std::vector<char>& payload,
    Schema::Types type,
    kio::Encoding encoding,
    kio::Compression compression,
    uint64_t row_count,
    uint64_t uncompressed_size) {

    const IColumnEncoding& column_encoding = GetEncoding(encoding);
    return column_encoding.Decode(payload, type, row_count);
}
