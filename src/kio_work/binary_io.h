#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <istream>
#include <ostream>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace bio {
template <typename T>
T ReadPod(std::istream& input, const std::string& name) {
    static_assert(std::is_trivially_copyable_v<T>);

    T value{};
    input.read(reinterpret_cast<char*>(&value), sizeof(value));
    if (!input) {
        throw std::runtime_error("Failed to read " + name);
    }
    return value;
}

template <typename T>
bool TryReadPod(std::istream& input, T& value, const std::string& name) {
    static_assert(std::is_trivially_copyable_v<T>);

    input.read(reinterpret_cast<char*>(&value), sizeof(value));
    if (input) {
        return true;
    }
    if (input.eof() && input.gcount() == 0) {
        return false;
    }
    throw std::runtime_error("Failed to read " + name);
}

template <typename T>
void WritePod(std::ostream& output, const T& value, const std::string& name) {
    static_assert(std::is_trivially_copyable_v<T>);

    output.write(reinterpret_cast<const char*>(&value), sizeof(value));
    if (!output.good()) {
        throw std::runtime_error("Failed to write " + name);
    }
}

inline void ReadBytes(std::istream& input, char* data, uint64_t size,
                      const std::string& name) {
    if (size == 0) {
        return;
    }

    input.read(data, static_cast<std::streamsize>(size));
    if (!input) {
        throw std::runtime_error("Failed to read " + name);
    }
}

inline void WriteBytes(std::ostream& output, const char* data, uint64_t size,
                       const std::string& name) {
    if (size == 0) {
        return;
    }

    output.write(data, static_cast<std::streamsize>(size));
    if (!output.good()) {
        throw std::runtime_error("Failed to write " + name);
    }
}

inline void EnsureAvailable(const std::string& buffer, size_t pos,
                            uint64_t size, const std::string& name) {
    if (pos > buffer.size() || size > buffer.size() - pos) {
        throw std::runtime_error("Corrupted KIO batch: " + name +
                                 " is out of bounds");
    }
}

template <typename T>
T ReadPodFromBuffer(const std::string& buffer, size_t& pos,
                    const std::string& name) {
    static_assert(std::is_trivially_copyable_v<T>);

    T value{};
    EnsureAvailable(buffer, pos, sizeof(value), name);
    std::memcpy(&value, buffer.data() + pos, sizeof(value));
    pos += sizeof(value);
    return value;
}
}  // namespace bio
