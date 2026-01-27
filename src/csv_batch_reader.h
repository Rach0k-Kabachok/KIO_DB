#pragma once

#include "csv_row_parser.h"
#include <vector>
#include <string>
#include <fstream>
#include <stdexcept>

class CSVBatchReader {
    using ParsedBatch = std::vector<std::vector<std::string> >;
    using ParsedRow = std::vector<std::string>;

    static constexpr size_t kBatchSizeBytes = 1 << 20;  // 1 MB
    std::ifstream csv_stream_;
    bool eof_reached_ = false;

public:
    explicit CSVBatchReader(const std::string &filename)
        : csv_stream_(filename, std::ios::binary) {
        if (!csv_stream_.is_open()) {
            throw std::runtime_error("Failed to open file: " + filename);
        }
    }

    CSVBatchReader(CSVBatchReader &&) = default;
    CSVBatchReader &operator=(CSVBatchReader &&) = default;

    CSVBatchReader(const CSVBatchReader &) = delete;
    CSVBatchReader &operator=(const CSVBatchReader &) = delete;

    ParsedBatch ParseNextBatch() {
        if (eof_reached_) {
            return {};
        }

        std::string buffer(kBatchSizeBytes, '\0');
        csv_stream_.read(buffer.data(), kBatchSizeBytes);

        if (csv_stream_.gcount() == 0) {
            eof_reached_ = true;
            return {};
        }

        buffer.resize(csv_stream_.gcount());
        if (buffer.back() != '\n') {
            std::string tail;
            std::getline(csv_stream_, tail);
            buffer += tail;
            buffer += '\n';
        }

        if (csv_stream_.eof()) {
            eof_reached_ = true;
        }

        ParsedBatch result;
        CSVRowParser row_parser(std::move(buffer));
        ParsedRow row;

        while (!(row = row_parser.ParseNext()).empty()) {
            result.emplace_back(std::move(row));
        }

        return result;
    }

    bool IsEOF() const {
        return eof_reached_;
    }

};
