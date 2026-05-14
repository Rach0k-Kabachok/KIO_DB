#include "transport/csv/csv_row_parser.h"

#include <stdexcept>
#include <string>
#include <utility>

#include "columnar_types.h"

CSVRowParser::CSVRowParser(std::string &&buffer)
    : buffer_(std::move(buffer)) {
}

ctp::ParsedRow CSVRowParser::ParseNext() {
    ctp::ParsedRow result;
    std::string elem;

    if (cur_pos_ >= buffer_.size()) {
        return result;
    }

    char symb = '\0';
    bool in_quote = false;
    for (; cur_pos_ < buffer_.size(); ++cur_pos_) {
        symb = buffer_[cur_pos_];

        if (symb == '"') {
            if (in_quote && cur_pos_ + 1 < buffer_.size() &&
                buffer_[cur_pos_ + 1] == '"') {
                elem += '"';
                ++cur_pos_;
            } else {
                in_quote = !in_quote;
            }
        } else if (symb == kSeparator) {
            if (in_quote) {
                elem += symb;
            } else {
                result.emplace_back(std::move(elem));
                elem.clear();
            }
        } else if (symb == '\n' && !in_quote) {
            if (!elem.empty() && elem.back() == '\r') {
                elem.pop_back();
            }
            result.emplace_back(std::move(elem));
            ++cur_pos_;
            return result;
        } else {
            elem += symb;
        }
    }

    if (in_quote) {
        throw std::invalid_argument("Missed quote in row\n" + elem);
    }

    result.emplace_back(std::move(elem));
    return result;
}
