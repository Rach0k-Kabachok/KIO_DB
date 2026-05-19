#include "transport/kio/binary_io.h"

namespace bio {

void ReadBytes(std::istream& input, char* data, uint64_t size) {
    if (size == 0) {
        return;
    }

    input.read(data, static_cast<std::streamsize>(size));
    if (!input) {
        throw std::runtime_error("Failed to read bytes");
    }
}

void WriteBytes(std::ostream& output, const char* data, uint64_t size) {
    if (size == 0) {
        return;
    }

    output.write(data, static_cast<std::streamsize>(size));
    if (!output.good()) {
        throw std::runtime_error("Failed to write bytes");
    }
}

std::string ReadString(std::istream& input) {
    const uint64_t size = ReadPod<uint64_t>(input);
    std::string value(size, '\0');
    ReadBytes(input, value.data(), size);
    return value;
}

void WriteString(std::ostream& output, const std::string& value) {
    WritePod(output, static_cast<uint64_t>(value.size()));
    WriteBytes(output, value.data(), value.size());
}

}  // namespace bio
