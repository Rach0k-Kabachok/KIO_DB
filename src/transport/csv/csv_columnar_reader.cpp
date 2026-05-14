#include "transport/csv/csv_columnar_reader.h"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "columnar_batch.h"
#include "transport/csv/csv_type_parser.h"

namespace {
template <typename T>
void AddTypedColumn(ctp::ColumnarBatch& batch, size_t reserve_rows) {
    std::vector<T> column;
    column.reserve(reserve_rows);
    batch.emplace_back(std::move(column));
}
}  // namespace

CSVColumnarReader::CSVColumnarReader(const std::string& data_name,
                                     const Schema& schema)
    : batch_reader_(data_name), schema_(schema) {
}

ctp::ColumnarBatch CSVColumnarReader::MakeEmptyBatch(size_t reserve_rows) const {
    ctp::ColumnarBatch batch;
    batch.reserve(schema_.GetColumnCount());

    for (size_t col_idx = 0; col_idx < schema_.GetColumnCount(); ++col_idx) {
        switch (schema_.SearchTypeByIndex(col_idx)) {
        case Schema::BIGINT:
        case Schema::TIMESTAMP:
            AddTypedColumn<int64_t>(batch, reserve_rows);
            break;
        case Schema::INTEGER:
        case Schema::DATE:
            AddTypedColumn<int32_t>(batch, reserve_rows);
            break;
        case Schema::SMALLINT:
            AddTypedColumn<int16_t>(batch, reserve_rows);
            break;
        case Schema::TEXT:
        case Schema::VARCHAR:
            AddTypedColumn<std::string>(batch, reserve_rows);
            break;
        case Schema::CHAR:
            AddTypedColumn<char>(batch, reserve_rows);
            break;
        }
    }

    return batch;
}

void CSVColumnarReader::AppendField(ctp::ColumnarBatch& batch, size_t col_idx,
                                    std::string_view field) const {
    switch (schema_.SearchTypeByIndex(col_idx)) {
    case Schema::BIGINT:
        std::get<std::vector<int64_t>>(batch[col_idx]).push_back(
            csv::ParseInteger<int64_t>(field));
        break;
    case Schema::INTEGER:
        std::get<std::vector<int32_t>>(batch[col_idx]).push_back(
            csv::ParseInteger<int32_t>(field));
        break;
    case Schema::SMALLINT:
        std::get<std::vector<int16_t>>(batch[col_idx]).push_back(
            csv::ParseInteger<int16_t>(field));
        break;
    case Schema::TEXT:
    case Schema::VARCHAR:
        std::get<std::vector<std::string>>(batch[col_idx])
            .emplace_back(field.data(), field.size());
        break;
    case Schema::CHAR:
        if (field.empty()) {
            throw std::invalid_argument("Empty char");
        }
        std::get<std::vector<char>>(batch[col_idx]).push_back(field[0]);
        break;
    case Schema::TIMESTAMP:
        std::get<std::vector<int64_t>>(batch[col_idx]).push_back(
            csv::TimeToSeconds(field));
        break;
    case Schema::DATE:
        std::get<std::vector<int32_t>>(batch[col_idx]).push_back(
            csv::DateToDays(field));
        break;
    }
}

void CSVColumnarReader::ParseBuffer(std::string&& buffer,
                                    ctp::ColumnarBatch& batch) const {
    std::string field;
    field.reserve(64);

    bool in_quote = false;
    bool row_started = false;
    size_t col_idx = 0;

    for (size_t i = 0; i < buffer.size(); ++i) {
        char ch = buffer[i];

        if (ch == '"') {
            row_started = true;
            if (in_quote && i + 1 < buffer.size() && buffer[i + 1] == '"') {
                field += '"';
                ++i;
            } else {
                in_quote = !in_quote;
            }
        } else if (ch == ',' && !in_quote) {
            AppendField(batch, col_idx, field);
            field.clear();
            ++col_idx;
            row_started = true;
        } else if (ch == '\n' && !in_quote) {
            if (!field.empty() && field.back() == '\r') {
                field.pop_back();
            }
            AppendField(batch, col_idx, field);
            field.clear();
            col_idx = 0;
            row_started = false;
        } else {
            field += ch;
            row_started = true;
        }
    }

    if (in_quote) {
        throw std::invalid_argument("Missed quote in row\n" + field);
    }

    if (!field.empty() || col_idx != 0 || row_started) {
        if (!field.empty() && field.back() == '\r') {
            field.pop_back();
        }
        AppendField(batch, col_idx, field);
    }
}

ctp::ColumnarBatch CSVColumnarReader::MakeColumnarBatch() {
    std::string buffer = batch_reader_.ReadNextBuffer();
    if (buffer.empty()) {
        return {};
    }

    ctp::ColumnarBatch batch = MakeEmptyBatch(buffer.size() / 128 + 1);
    ParseBuffer(std::move(buffer), batch);
    ctp::ValidateColumnarBatch(batch, schema_);
    return batch;
}
