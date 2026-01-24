#pragma once

#include <optional>
#include <vector>
#include <string>
#include <fstream>

class CSVRowParser {
    using ParsedRow = std::vector<std::string>;

    std::ifstream csv_stream;

public:
    explicit CSVRowParser(const std::string &filename) : csv_stream(filename, std::ios::binary) {
    }

    std::optional<ParsedRow> ParseNext() {
        ParsedRow result;
        std::string cur_line;
        std::string elem;

        if (!std::getline(csv_stream, cur_line)) {
            if (csv_stream.eof()) {
                return std::nullopt;
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
                } else if (cur_line[i + 1] == ',') {
                    in_quote = false;
                    result.emplace_back(std::move(elem));
                    i++;
                } else {
                    throw std::invalid_argument("Missed quote in row\n" + cur_line);
                }
            } else if (symb == ',') {
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
