#include "csv_work/csv_formatter.h"

#include <charconv>
#include <chrono>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <system_error>
#include <type_traits>
#include <vector>
#include <string>

#include "columnar_batch.h"
#include "columnar_types.h"
#include "csv_work/csv_batch_reader.h"
#include "schema.h"

namespace {
    void CheckDateSize(const std::string& value, size_t expected_size,
                       size_t row_idx, size_t col_idx) {
        if (value.size() < expected_size) {
            throw std::invalid_argument(
                "Invalid date/time at row " + std::to_string(row_idx) +
                " col " + std::to_string(col_idx) + ": " + value);
        }
    }

    std::chrono::sys_days ParseDateDays(const std::string& value) {
        const int year = (value[0] - '0') * 1000 + (value[1] - '0') * 100 +
                         (value[2] - '0') * 10 + (value[3] - '0');
        const unsigned month = (value[5] - '0') * 10 + (value[6] - '0');
        const unsigned day = (value[8] - '0') * 10 + (value[9] - '0');

        return std::chrono::sys_days{
            std::chrono::year{year} / std::chrono::month{month} /
            std::chrono::day{day}};
    }

    std::int64_t ParseTimestampSeconds(const std::string& value) {
        const int hour = (value[11] - '0') * 10 + (value[12] - '0');
        const int minute = (value[14] - '0') * 10 + (value[15] - '0');
        const int second = (value[17] - '0') * 10 + (value[18] - '0');

        const auto time = std::chrono::sys_seconds{
            ParseDateDays(value).time_since_epoch()} +
                          std::chrono::hours{hour} +
                          std::chrono::minutes{minute} +
                          std::chrono::seconds{second};

        return time.time_since_epoch().count();
    }

    template<typename T>
    T ParseInteger(const std::string& value, size_t row_idx, size_t col_idx) {
        static_assert(std::is_integral_v<T>, "T must be an integral type");

        if (value.empty()) {
            throw std::invalid_argument(
                "Empty integer at row " + std::to_string(row_idx) +
                " col " + std::to_string(col_idx));
        }

        long long parsed = 0;
        const char* begin = value.data();
        const char* end = value.data() + value.size();
        const auto [ptr, ec] = std::from_chars(begin, end, parsed);
        if (ec != std::errc() || ptr != end ||
            parsed < std::numeric_limits<T>::min() ||
            parsed > std::numeric_limits<T>::max()) {
            throw std::invalid_argument(
                "Invalid integer at row " + std::to_string(row_idx) +
                " col " + std::to_string(col_idx) + ": " + value);
        }

        return static_cast<T>(parsed);
    }

    template<typename T>
    std::vector<T> ParseNumColumn(size_t num_rows, size_t col_idx, const ctp::ParsedBatch& batch) {
        static_assert(std::is_integral_v<T>, "T must be an integral type");

        std::vector<T> result;
        result.reserve(num_rows);

        for (size_t row_idx = 0; row_idx < num_rows; ++row_idx) {
            result.push_back(ParseInteger<T>(batch[row_idx][col_idx],
                                             row_idx, col_idx));
        }

        return result;
    }

    template<typename T>
    std::vector<T> ParseStringColumn(size_t num_rows, size_t col, const ctp::ParsedBatch& batch) {
        std::vector<T> result;
        result.reserve(num_rows);

        for (size_t row_idx = 0; row_idx < num_rows; ++row_idx) {
            if constexpr (std::is_same_v<T, char>) {
                if (batch[row_idx][col].empty()) {
                    throw std::invalid_argument(
                        "Empty char at row " + std::to_string(row_idx) +
                        " col " + std::to_string(col));
                }
                result.push_back(batch[row_idx][col][0]);
            } else {
                result.push_back(static_cast<T>(batch[row_idx][col]));
            }
        }
        return result;
    }

    std::vector<int64_t> ParseTimeStampColumn(size_t num_rows, size_t col, const ctp::ParsedBatch& batch) {
        std::vector<int64_t> result;
        result.reserve(num_rows);

        for (size_t row_idx = 0; row_idx < num_rows; ++row_idx) {
            CheckDateSize(batch[row_idx][col], 19, row_idx, col);
            result.push_back((ParseTimestampSeconds(batch[row_idx][col])));
        }
        return result;
    }

    std::vector<int32_t> ParseDateColumn(size_t num_rows, size_t col, const ctp::ParsedBatch& batch) {
        std::vector<int32_t> result;
        result.reserve(num_rows);

        for (size_t row_idx = 0; row_idx < num_rows; ++row_idx) {
            CheckDateSize(batch[row_idx][col], 10, row_idx, col);
            result.push_back(ParseDateDays(batch[row_idx][col]).time_since_epoch().count());
        }
        return result;
    }
}

CSVFormatter::CSVFormatter(const std::string &data_name, const Schema &schema)
        : batch_reader_(data_name), schema_(schema) {
}

ctp::ColumnarBatch CSVFormatter::MakeColumnarBatch() {
    ctp::ParsedBatch batch = batch_reader_.ParseNextBatch();

    if (batch.empty()) {
        return {};
    }

    ctp::ValidateParsedBatch(batch, schema_);

    size_t num_rows = batch.size();
    size_t num_cols = schema_.GetColumnCount();

    ctp::ColumnarBatch result;
    result.reserve(num_cols);

    for (size_t col_idx = 0; col_idx < num_cols; ++col_idx) {
        auto type = schema_.SearchTypeByIndex(col_idx);

        switch (type) {
        case Schema::BIGINT:
            result.emplace_back(ParseNumColumn<int64_t>(num_rows, col_idx, batch));
            break;
        case Schema::INTEGER:
            result.emplace_back(ParseNumColumn<int32_t>(num_rows, col_idx, batch));
            break;
        case Schema::SMALLINT: 
            result.emplace_back(ParseNumColumn<int16_t>(num_rows, col_idx, batch));
            break;
        case Schema::TEXT:
            result.emplace_back(ParseStringColumn<std::string>(num_rows, col_idx, batch));
            break;
        case Schema::VARCHAR:
            result.emplace_back(ParseStringColumn<std::string>(num_rows, col_idx, batch));
            break;
        case Schema::CHAR:
            result.emplace_back(ParseStringColumn<char>(num_rows, col_idx, batch));
            break;
        case Schema::TIMESTAMP:
            result.emplace_back(ParseTimeStampColumn(num_rows, col_idx, batch));
            break;
        case Schema::DATE:
            result.emplace_back(ParseDateColumn(num_rows, col_idx, batch));
            break;
        }
    }

    return result;
}
