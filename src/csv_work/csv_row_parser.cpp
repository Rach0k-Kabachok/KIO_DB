#include "csv_work/csv_row_parser.h"

#include <stdexcept>
#include <string>
#include <utility>

#include "columnar_types.h"

std::string CSVRowParser::GetLine() {
    std::string result;
    bool in_quote = false;

    for (size_t i = cur_pos_; i < buffer_.size(); i++) {
        if (buffer_[i] == '"') {
            result += '"';
            if (in_quote && (i + 1 < buffer_.size() && buffer_[i + 1] == '"')) {
                result += buffer_[++i];
            } else {
                in_quote = !in_quote;
            }

        } else if (buffer_[i] == '\n' && !in_quote) {
            if (!result.empty() && result.back() == '\r') {
                result.pop_back();
            }
            cur_pos_ = i + 1;
            return result;
        } else {
            result += buffer_[i];
        }
    }

    if (in_quote) {
        throw std::invalid_argument("Missed quote in row\n" + result);
    }
    
    cur_pos_ = buffer_.size();
    return result;
}

CSVRowParser::CSVRowParser(std::string &&buffer)
    : buffer_(std::move(buffer)) {
}

ctp::ParsedRow CSVRowParser::ParseNext() {
    ctp::ParsedRow result;
    std::string elem;

    if (cur_pos_ >= buffer_.size()) {
        return result;
    }

    std::string cur_line = GetLine();

    char symb = '\0';
    bool in_quote = false;
    for (size_t i = 0; i < cur_line.size(); ++i) {
        symb = cur_line[i];

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
                throw std::invalid_argument("Missed quote in row\n" + cur_line);
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
