#pragma once

#include <vector>
#include <string>
#include <variant>
#include <cstdint>

namespace ctp {
using Date = std::int32_t;
using Timestamp = std::int64_t;
using Varchar = std::string;

using ParsedRow = std::vector<std::string>;
using ParsedBatch = std::vector<std::vector<std::string> >;
using Column = std::variant<std::vector<int64_t>, std::vector<int32_t>, std::vector<int16_t>, 
                            std::vector<std::string>,
                            std::vector<char>, std::vector<unsigned char>>;
using ColumnarBatch = std::vector<Column>;
}  // namespace ctp
