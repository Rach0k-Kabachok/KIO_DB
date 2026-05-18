#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>
#include <vector>

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

inline void AppendBytes(std::vector<char>& output, const char* data,
                        size_t size) {
    const size_t offset = output.size();
    output.resize(offset + size);
    if (size != 0) {
        std::memcpy(output.data() + offset, data, size);
    }
}

inline void AppendString(std::vector<char>& output, const std::string& value) {
    AppendPod(output, static_cast<uint64_t>(value.size()));
    AppendBytes(output, value.data(), value.size());
}

inline std::string ReadString(const std::vector<char>& input, size_t& offset) {
    const uint64_t size = ReadPod<uint64_t>(input, offset);
    std::string result(size, '\0');
    if (size != 0) {
        std::memcpy(result.data(), input.data() + offset, size);
    }
    offset += size;
    return result;
}
