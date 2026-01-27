#pragma once

#include <vector>
#include <string>
#include <sstream>
#include <stdexcept>

class CSVRowParser {
    using ParsedRow = std::vector<std::string>;

    static constexpr char kSeparator = ',';
    std::istringstream buffer_stream_;

public:
    explicit CSVRowParser(std::string &&buffer)
        : buffer_stream_(std::move(buffer)) {
    }

    ParsedRow ParseNext() {
        ParsedRow result;
        std::string cur_line;
        std::string elem;

        if (!std::getline(buffer_stream_, cur_line)) {
            if (buffer_stream_.eof()) {
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
                } else if (cur_line[i + 1] == kSeparator) {
                    in_quote = false;
                    result.emplace_back(std::move(elem));
                    i++;
                } else {
                    throw std::invalid_argument("Missed quote in row\n" +
                                                cur_line);
                }
            } else if (symb == kSeparator) {
                if (in_quote) {
                    elem += symb;
                } else {
                    result.emplace_back(std::move(elem));
                    elem.clear();
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
