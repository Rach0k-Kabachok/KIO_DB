#include "transport/compression/binary_codec.h"

namespace bcodec {

void AppendBytes(std::vector<char>& output, const char* data, size_t size) {
    const size_t offset = output.size();
    output.resize(offset + size);
    if (size != 0) {
        std::memcpy(output.data() + offset, data, size);
    }
}

void AppendString(std::vector<char>& output, const std::string& value) {
    AppendPod(output, static_cast<uint64_t>(value.size()));
    AppendBytes(output, value.data(), value.size());
}

std::string ReadString(const std::vector<char>& input, size_t& offset) {
    const uint64_t size = ReadPod<uint64_t>(input, offset);
    std::string result(size, '\0');
    if (size != 0) {
        std::memcpy(result.data(), input.data() + offset, size);
    }
    offset += size;
    return result;
}

}  // namespace bcodec
