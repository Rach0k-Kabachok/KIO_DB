#pragma once

#include <cstdint>
#include <istream>
#include <ostream>
#include <stdexcept>
#include <type_traits>

namespace bio {
template <typename T>
T ReadPod(std::istream& input) {
    static_assert(std::is_trivially_copyable_v<T>);

    T value{};
    input.read(reinterpret_cast<char*>(&value), sizeof(value));
    if (!input) {
        throw std::runtime_error("Failed to read binary value");
    }
    return value;
}

template <typename T>
void WritePod(std::ostream& output, const T& value) {
    static_assert(std::is_trivially_copyable_v<T>);

    output.write(reinterpret_cast<const char*>(&value), sizeof(value));
    if (!output.good()) {
        throw std::runtime_error("Failed to write binary value");
    }
}

inline void ReadBytes(std::istream& input, char* data, uint64_t size) {
    if (size == 0) {
        return;
    }

    input.read(data, static_cast<std::streamsize>(size));
    if (!input) {
        throw std::runtime_error("Failed to read bytes");
    }
}

inline void WriteBytes(std::ostream& output, const char* data, uint64_t size) {
    if (size == 0) {
        return;
    }

    output.write(data, static_cast<std::streamsize>(size));
    if (!output.good()) {
        throw std::runtime_error("Failed to write bytes");
    }
}

}  // namespace bio
