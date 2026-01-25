#pragma once

#include <vector>
#include <string>
#include <sstream>

class CSVRowParser {
    using ParsedRow = std::vector<std::string>;

    static constexpr char separator = ',';
    std::istringstream buffer_stream;

public:
    explicit CSVRowParser(const std::string &filename) : buffer_stream(filename, std::ios::binary) {
    }

    ParsedRow ParseNext() {
        ParsedRow result;
        std::string cur_line;
        std::string elem;

        if (!std::getline(buffer_stream, cur_line)) {
            if (buffer_stream.eof()) {
                return {};
            }
            throw std::runtime_error("std::getline failed");
        }

        bool in_quote = false;
        for (size_t i = 0; i < cur_line.size(); ++i) {
            char symb = cur_line[i];

            if (symb == '"') {
                if (!in_quote) {
                    in_quote = true;
                } else if (i + 1 == cur_line.size()) {
                    in_quote = false;
                } else if (cur_line[i + 1] == '"') {
                    elem += '"';
                    i++;
                } else if (cur_line[i + 1] == separator) {
                    in_quote = false;
                    result.emplace_back(std::move(elem));
                    i++;
                } else {
                    throw std::invalid_argument("Missed quote in row\n" + cur_line);
                }
            } else if (symb == separator) {
                if (in_quote) {
                    elem += symb;
                } else {
                    result.emplace_back(std::move(elem));
                }
            } else {
                elem += symb;
            }
        }

        if (in_quote) {
            throw std::invalid_argument("Missed quote in row\n" + cur_line);
        }

        result.emplace_back(std::move(elem));
        return result;
    }
};
