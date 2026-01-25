#pragma once

#include "csv_row_parser.h"
#include <vector>
#include <string>
#include <fstream>

class CSVBatchReader {
public:
  using ParsedBatch = std::vector<std::vector<std::string> >;

private:
  static constexpr size_t batch_size_bytes = 1 << 23; // 1 MB
  std::ifstream csv_stream;
  bool eof_reached = false;

public:
  explicit CSVBatchReader(const std::string &filename)
    : csv_stream(filename, std::ios::binary) {
    if (!csv_stream.is_open()) {
      throw std::runtime_error("Failed to open file: " + filename);
    }
  }

  ParsedBatch ParseNextBatch() {
    if (eof_reached) {
      return {};
    }

    std::string buffer(batch_size_bytes, '\0');
    csv_stream.read(buffer.data(), batch_size_bytes);

    if (csv_stream.gcount() == 0) {
      eof_reached = true;
      return {};
    }

    if (buffer.back() != '\n') {
      std::string tail;
      std::getline(csv_stream, tail);
      buffer += tail;
    }

    if (csv_stream.eof()) {
      eof_reached = true;
    }

    ParsedBatch result;
    CSVRowParser row_parser(buffer);
    std::vector<std::string> row;

    while (!(row = row_parser.ParseNext()).empty()) {
      result.emplace_back(std::move(row));
    }

    return result;
  }

  bool IsEOF() const {
    return eof_reached;
  }
};
