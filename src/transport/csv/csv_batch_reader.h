#pragma once

#include <cstddef>
#include <fstream>
#include <string>

#include "columnar_types.h"

class CSVBatchReader {
    static constexpr size_t kBatchSizeBytes = 64 * (1 << 20);  // 64 МБ
    std::ifstream csv_stream_;
    bool eof_reached_ = false;
    std::string pending_buffer_;

public:
    explicit CSVBatchReader(const std::string &filename);

    CSVBatchReader(const CSVBatchReader &) = delete;
    CSVBatchReader &operator=(const CSVBatchReader &) = delete;

    CSVBatchReader(CSVBatchReader &&) = default;
    CSVBatchReader &operator=(CSVBatchReader &&) = default;

    ctp::ParsedBatch ParseNextBatch();

    std::string ReadNextBuffer();

    bool IsEOF() const;

private:
    size_t CutBatchToNewLine(const std::string &buffer);

    ctp::ParsedBatch ParseBuffer(std::string &&buffer);
};
