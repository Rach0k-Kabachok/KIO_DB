#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>
#include <vector>

namespace bcodec {

template <typename T>
void AppendPod(std::vector<char>& output, const T& value) {
    static_assert(std::is_trivially_copyable_v<T>);

    const size_t offset = output.size();
    output.resize(offset + sizeof(T));
    std::memcpy(output.data() + offset, &value, sizeof(T));
}

template <typename T>
T ReadPod(const std::vector<char>& input, size_t& offset) {
    static_assert(std::is_trivially_copyable_v<T>);

    T value{};
    std::memcpy(&value, input.data() + offset, sizeof(T));
    offset += sizeof(T);
    return value;
}

void AppendBytes(std::vector<char>& output, const char* data, size_t size);
void AppendString(std::vector<char>& output, const std::string& value);
std::string ReadString(const std::vector<char>& input, size_t& offset);

}  // namespace bcodec
