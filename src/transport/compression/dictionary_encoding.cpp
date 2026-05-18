#include "transport/compression/dictionary_encoding.h"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "transport/compression/binary_codec.h"
#include "transport/compression/bit_packing_encoding.h"
#include "transport/kio/kio_serialization.h"

namespace {

std::vector<char> EncodeStringColumn(
    const std::vector<std::string>& values) {
    std::unordered_map<std::string, uint64_t> dictionary_index;
    std::vector<std::string> dictionary;
    std::vector<uint64_t> ids;
    dictionary.reserve(values.size());
    ids.reserve(values.size());

    for (const std::string& value : values) {
        auto [it, inserted] =
            dictionary_index.try_emplace(value, dictionary.size());
        if (inserted) {
            dictionary.push_back(value);
        }
        ids.push_back(it->second);
    }

    std::vector<char> result;
    AppendPod(
        result, static_cast<uint64_t>(dictionary.size()));

    std::vector<char> dictionary_payload =
        kio::SerializeStringColumn(dictionary);
    result.insert(result.end(), dictionary_payload.begin(),
                  dictionary_payload.end());

    std::vector<char> ids_payload = EncodeUnsignedValues(ids);
    result.insert(result.end(), ids_payload.begin(), ids_payload.end());
    return result;
}

std::vector<std::string> DecodeDictionary(
    const std::vector<char>& payload,
    size_t& offset,
    uint64_t dictionary_size) {
    std::vector<uint64_t> sizes(dictionary_size);
    const size_t sizes_bytes = sizes.size() * sizeof(uint64_t);
    if (sizes_bytes != 0) {
        std::memcpy(sizes.data(), payload.data() + offset, sizes_bytes);
    }
    offset += sizes_bytes;

    std::vector<std::string> dictionary;
    dictionary.reserve(dictionary_size);
    for (uint64_t size : sizes) {
        std::string value(size, '\0');
        if (size != 0) {
            std::memcpy(value.data(), payload.data() + offset,
                        size);
        }
        offset += size;
        dictionary.push_back(std::move(value));
    }

    return dictionary;
}

ctp::Column DecodeStringColumn(const std::vector<char>& payload,
                               uint64_t row_count) {
    size_t offset = 0;
    const uint64_t dictionary_size =
        ReadPod<uint64_t>(payload, offset);
    std::vector<std::string> dictionary =
        DecodeDictionary(payload, offset, dictionary_size);
    std::vector<uint64_t> ids =
        DecodeUnsignedValues(payload, offset, row_count);

    std::vector<std::string> result;
    result.reserve(row_count);
    for (uint64_t id : ids) {
        result.push_back(dictionary[id]);
    }
    return result;
}

}  // namespace

kio::Encoding DictionaryEncoding::Kind() const {
    return kio::Encoding::DICTIONARY;
}

std::vector<char> DictionaryEncoding::Encode(
    const ctp::Column& column,
    Schema::Types type) const {
    switch (type) {
    case Schema::TEXT:
    case Schema::VARCHAR:
        return EncodeStringColumn(std::get<std::vector<std::string>>(column));
    case Schema::BIGINT:
    case Schema::INTEGER:
    case Schema::SMALLINT:
    case Schema::CHAR:
    case Schema::TIMESTAMP:
    case Schema::DATE:
        break;
    }

    throw std::invalid_argument("Dictionary encoding supports only strings");
}

ctp::Column DictionaryEncoding::Decode(
    const std::vector<char>& payload,
    Schema::Types type,
    uint64_t row_count) const {
    switch (type) {
    case Schema::TEXT:
    case Schema::VARCHAR:
        return DecodeStringColumn(payload, row_count);
    case Schema::BIGINT:
    case Schema::INTEGER:
    case Schema::SMALLINT:
    case Schema::CHAR:
    case Schema::TIMESTAMP:
    case Schema::DATE:
        break;
    }

    throw std::invalid_argument("Dictionary encoding supports only strings");
}
