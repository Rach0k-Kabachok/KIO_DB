#include "transport/compression/rle_encoding.h"

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include "schema.h"
#include "transport/compression/binary_codec.h"

namespace {

template <typename T>
std::vector<char> EncodeTypedColumn(const std::vector<T>& values) {
    std::vector<char> result;
    if (values.empty()) {
        bcodec::AppendPod(result, uint64_t{0});
        return result;
    }

    std::vector<std::pair<T, uint64_t>> runs;
    runs.reserve(values.size());
    T current = values.front();
    uint64_t run_length = 1;

    for (size_t idx = 1; idx < values.size(); ++idx) {
        if (values[idx] == current) {
            ++run_length;
        } else {
            runs.emplace_back(current, run_length);
            current = values[idx];
            run_length = 1;
        }
    }
    runs.emplace_back(current, run_length);

    bcodec::AppendPod(result, static_cast<uint64_t>(runs.size()));
    for (const auto& [value, length] : runs) {
        bcodec::AppendPod(result, value);
        bcodec::AppendPod(result, length);
    }

    return result;
}

std::vector<char> EncodeStringColumn(const std::vector<std::string>& values) {
    std::vector<char> result;
    if (values.empty()) {
        bcodec::AppendPod(result, uint64_t{0});
        return result;
    }

    std::vector<std::pair<std::string, uint64_t>> runs;
    runs.reserve(values.size());
    std::string current = values.front();
    uint64_t run_length = 1;

    for (size_t idx = 1; idx < values.size(); ++idx) {
        if (values[idx] == current) {
            ++run_length;
        } else {
            runs.emplace_back(std::move(current), run_length);
            current = values[idx];
            run_length = 1;
        }
    }
    runs.emplace_back(std::move(current), run_length);

    bcodec::AppendPod(result, static_cast<uint64_t>(runs.size()));
    for (const auto& [value, length] : runs) {
        bcodec::AppendString(result, value);
        bcodec::AppendPod(result, length);
    }

    return result;
}

template <typename T>
ctp::Column DecodeTypedColumn(const std::vector<char>& payload,
                              uint64_t row_count) {
    size_t offset = 0;
    const uint64_t run_count =
        bcodec::ReadPod<uint64_t>(payload, offset);

    std::vector<T> result;
    result.reserve(row_count);
    for (uint64_t idx = 0; idx < run_count; ++idx) {
        const T value = bcodec::ReadPod<T>(payload, offset);
        const uint64_t length =
            bcodec::ReadPod<uint64_t>(payload, offset);
        result.insert(result.end(), length, value);
    }

    return result;
}

ctp::Column DecodeStringColumn(const std::vector<char>& payload,
                               uint64_t row_count) {
    size_t offset = 0;
    const uint64_t run_count =
        bcodec::ReadPod<uint64_t>(payload, offset);

    std::vector<std::string> result;
    result.reserve(row_count);
    for (uint64_t idx = 0; idx < run_count; ++idx) {
        const std::string value =
            bcodec::ReadString(payload, offset);
        const uint64_t length =
            bcodec::ReadPod<uint64_t>(payload, offset);
        result.insert(result.end(), length, value);
    }

    return result;
}

}  // namespace

kio::Encoding RleEncoding::Kind() const {
    return kio::Encoding::RLE;
}

std::vector<char> RleEncoding::Encode(
    const ctp::Column& column,
    Schema::Types type) const {
    return std::visit([](const auto& values) -> std::vector<char> {
        using Values = std::decay_t<decltype(values)>;
        using Value = typename Values::value_type;

        if constexpr (std::is_same_v<Value, std::string>) {
            return EncodeStringColumn(values);
        } else {
            return EncodeTypedColumn(values);
        }
    }, column);
}

ctp::Column RleEncoding::Decode(
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
        return DecodeTypedColumn<char>(payload, row_count);
    case Schema::TEXT:
    case Schema::VARCHAR:
        return DecodeStringColumn(payload, row_count);
    case Schema::DOUBLE:
        return DecodeTypedColumn<double>(payload, row_count);
    }

    throw std::invalid_argument("Unsupported schema type");
}
