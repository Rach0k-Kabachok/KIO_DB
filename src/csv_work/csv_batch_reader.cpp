#include "csv_work/csv_batch_reader.h"

#include <vector>
#include <string>
#include <fstream>

#include "columnar_types.h"
#include "csv_work/csv_row_parser.h"

CSVBatchReader::CSVBatchReader(const std::string &filename)
        : csv_stream_(filename, std::ios::binary) {
    if (!csv_stream_.is_open()) {
        throw std::runtime_error("Failed to open file: " + filename);
    }
}

ctp::ParsedBatch CSVBatchReader::ParseNextBatch() {
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

    ctp::ParsedBatch result;
    CSVRowParser row_parser(std::move(buffer));
    ctp::ParsedRow row;

    while (!(row = row_parser.ParseNext()).empty()) {
        result.emplace_back(std::move(row));
    }

    return result;
}


bool CSVBatchReader::IsEOF() const {
    return eof_reached_;
}