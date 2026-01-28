#pragma once

#include <string>
#include <sstream>

#include "columnar_types.h"


class CSVRowParser {
    static constexpr char kSeparator = ',';
    std::istringstream buffer_stream_;

public:
    explicit CSVRowParser(std::string &&buffer);

    ctp::ParsedRow ParseNext();
};
