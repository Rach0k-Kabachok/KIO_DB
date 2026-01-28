#pragma once

#include <fstream>

#include "columnar_types.h"

class CSVBatchReader {
    static constexpr size_t kBatchSizeBytes = 1 << 20;  // 1 MB
    std::ifstream csv_stream_;
    bool eof_reached_ = false;

public:
    explicit CSVBatchReader(const std::string &filename);

    CSVBatchReader(const CSVBatchReader &) = delete;
    CSVBatchReader &operator=(const CSVBatchReader &) = delete;

    CSVBatchReader(CSVBatchReader &&) = default;
    CSVBatchReader &operator=(CSVBatchReader &&) = default;

    ctp::ParsedBatch ParseNextBatch() ;

    bool IsEOF() const;
};
