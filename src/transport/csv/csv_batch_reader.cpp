#include "transport/csv/csv_batch_reader.h"

#include <cstddef>
#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "columnar_types.h"
#include "transport/csv/csv_row_parser.h"

size_t CSVBatchReader::CutBatchToNewLine(const std::string &buffer) {
    bool in_quote = false;
    size_t last_row_end = std::string::npos;

    for (size_t i = 0; i < buffer.size(); ++i) {
        if (buffer[i] == '"') {
            if (in_quote && i + 1 < buffer.size() && buffer[i + 1] == '"') {
                ++i;
            } else {
                in_quote = !in_quote;
            }
        } else if (buffer[i] == '\n' && !in_quote) {
            last_row_end = i;
        }
    }

    return last_row_end;
}

ctp::ParsedBatch CSVBatchReader::ParseBuffer(std::string &&buffer) {
    ctp::ParsedBatch result;
    CSVRowParser row_parser(std::move(buffer));
    ctp::ParsedRow row;

    while (!(row = row_parser.ParseNext()).empty()) {
        result.emplace_back(std::move(row));
    }

    return result;
}

CSVBatchReader::CSVBatchReader(const std::string &filename)
    : csv_stream_(filename, std::ios::binary) {
    if (!csv_stream_.is_open()) {
        throw std::runtime_error("Failed to open file: " + filename);
    }
}

ctp::ParsedBatch CSVBatchReader::ParseNextBatch() {
    std::string buffer = ReadNextBuffer();
    if (buffer.empty()) {
        return {};
    }

    return ParseBuffer(std::move(buffer));
}

std::string CSVBatchReader::ReadNextBuffer() {
    if (eof_reached_) {
        return {};
    }

    std::string buffer = std::move(pending_buffer_);
    pending_buffer_.clear();

    while (true) { // читаем, пока не найдем конец строки или конец файла
        const size_t initial_size = buffer.size();
        buffer.resize(initial_size + kBatchSizeBytes);

        csv_stream_.read(buffer.data() + initial_size, kBatchSizeBytes);
        if (csv_stream_.bad()) {
            throw std::runtime_error("Failed to read CSV batch");
        }

        buffer.resize(initial_size + csv_stream_.gcount());

        if (csv_stream_.gcount() == 0) {
            eof_reached_ = true;
            if (buffer.empty()) {
                return {};
            }
            return buffer;
        }

        if (csv_stream_.eof()) {
            eof_reached_ = true;
            return buffer;
        }


        const size_t last_row_end = CutBatchToNewLine(buffer);
        if (last_row_end != std::string::npos) {
            pending_buffer_ = buffer.substr(last_row_end + 1);
            buffer.resize(last_row_end + 1);
            return buffer;
        }
    }
}

bool CSVBatchReader::IsEOF() const {
    return eof_reached_;
}
