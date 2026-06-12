#include "transport/compression/delta_encoding.h"

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include "transport/compression/binary_codec.h"
#include "transport/compression/bit_packing_encoding.h"

namespace {

template <typename T>
std::vector<char> EncodeTypedColumn(const std::vector<T>& values) {
    std::vector<char> result;
    if (values.empty()) {
        return result;
    }

    int64_t previous = static_cast<int64_t>(values.front());
    bcodec::AppendPod(result, previous);

    std::vector<uint64_t> deltas;
    deltas.reserve(values.size() - 1);
    for (size_t idx = 1; idx < values.size(); ++idx) {
        const int64_t current = static_cast<int64_t>(values[idx]);
        deltas.push_back(ZigZagEncode(current - previous));
        previous = current;
    }

    std::vector<char> packed_deltas =
        EncodeUnsignedValues(deltas);
    result.insert(result.end(), packed_deltas.begin(), packed_deltas.end());
    return result;
}

template <typename T>
ctp::Column DecodeTypedColumn(const std::vector<char>& payload,
                              uint64_t row_count) {
    std::vector<T> result;
    result.reserve(row_count);
    if (row_count == 0) {
        return result;
    }

    size_t offset = 0;
    int64_t current = bcodec::ReadPod<int64_t>(payload, offset);
    result.push_back(static_cast<T>(current));

    std::vector<uint64_t> deltas =
        DecodeUnsignedValues(payload, offset, row_count - 1);
    for (uint64_t encoded_delta : deltas) {
        current += ZigZagDecode(encoded_delta);
        result.push_back(static_cast<T>(current));
    }

    return result;
}

}  // namespace

kio::Encoding DeltaEncoding::Kind() const {
    return kio::Encoding::DELTA;
}

std::vector<char> DeltaEncoding::Encode(
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
    case Schema::TEXT:
    case Schema::VARCHAR:
    case Schema::DOUBLE:
        return EncodeTypedColumn(std::get<std::vector<double>>(column));
    }

    throw std::invalid_argument("Delta encoding supports only numeric types");
}

ctp::Column DeltaEncoding::Decode(
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
    case Schema::TEXT:
    case Schema::VARCHAR:
    case Schema::DOUBLE:
        break;
    }

    throw std::invalid_argument("Delta encoding supports only numeric types");
}
