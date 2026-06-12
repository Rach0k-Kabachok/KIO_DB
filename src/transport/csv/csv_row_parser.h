#pragma once

#include <string>

#include "columnar_types.h"


class CSVRowParser {
    static constexpr char kSeparator = ',';
    std::string buffer_;
    size_t cur_pos_ = 0;

public:
    explicit CSVRowParser(std::string &&buffer);

    ctp::ParsedRow ParseNext();
};
