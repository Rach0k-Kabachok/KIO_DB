#include "transport/csv/csv_exporter.h"

#include <cstdint>
#include <iomanip>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <vector>

#include "columnar_batch.h"
#include "column_operations.h"
#include "transport/kio/kio_db_reader.h"

namespace {
constexpr int64_t kBaseYear = 1970;
constexpr int64_t kMonthsPerYear = 12;
constexpr int64_t kDaysPerMonth = 31;
constexpr int64_t kHoursPerDay = 24;
constexpr int64_t kMinutesPerHour = 60;
constexpr int64_t kSecondsPerMinute = 60;
constexpr int64_t kSecondsPerDay =
    kHoursPerDay * kMinutesPerHour * kSecondsPerMinute;

bool ShouldQuoteColumn(Schema::Types type) {
    switch (type) {
    case Schema::BIGINT:
    case Schema::INTEGER:
    case Schema::SMALLINT:
        return false;
    case Schema::TEXT:
    case Schema::VARCHAR:
    case Schema::CHAR:
    case Schema::TIMESTAMP:
    case Schema::DATE:
        return true;
    }

    throw std::invalid_argument("Invalid schema type");
}

void WriteEscapedString(std::ofstream& csv_file, const std::string& value) {
    for (char ch : value) {
        if (ch == '"') {
            csv_file << "\"\"";
        } else {
            csv_file << ch;
        }
    }
}

std::string FormatDateCode(std::int64_t days) {
    const int64_t day = days % kDaysPerMonth + 1;
    days /= kDaysPerMonth;
    const int64_t month = days % kMonthsPerYear + 1;
    const int64_t year = kBaseYear + days / kMonthsPerYear;

    std::ostringstream out;
    out << year << '-' << std::setw(2) << std::setfill('0') << month << '-'
        << std::setw(2) << std::setfill('0') << day;
    return out.str();
}

std::string FormatDate(std::int32_t days) {
    return FormatDateCode(days);
}

std::string FormatTimestamp(std::int64_t seconds) {
    const int64_t date = seconds / kSecondsPerDay;
    int64_t time = seconds % kSecondsPerDay;
    const int64_t hour = time / (kMinutesPerHour * kSecondsPerMinute);
    time %= kMinutesPerHour * kSecondsPerMinute;
    const int64_t minute = time / kSecondsPerMinute;
    const int64_t second = time % kSecondsPerMinute;

    std::ostringstream out;
    out << FormatDateCode(date) << ' ' << std::setw(2) << std::setfill('0')
        << hour << ':' << std::setw(2) << std::setfill('0') << minute << ':'
        << std::setw(2) << std::setfill('0') << second;
    return out.str();
}
}  // namespace

CsvExporter::CsvExporter(KioDbReader& reader, const std::string& csv_filename)
        : csv_name_(csv_filename), kio_reader_(reader) {
    csv_file_.open(csv_name_, std::ios::out);
    if (!csv_file_.is_open()) {
        throw std::runtime_error("Cannot open CSV file for writing: " + csv_name_);
    }
}

void CsvExporter::Export() {
    size_t batch_count = 0;
    while (true) {
        ctp::ColumnarBatch batch = kio_reader_.ReadNextBatch();
        if (batch.empty()) {
            break;
        }
        WriteBatchToStream(batch);
        batch_count++;
    }

    if (batch_count == 0) {
        std::cerr << "No batches to export" << std::endl;
        return;
    }

    std::cout << "Successfully exported " << batch_count << " batches to "
              << csv_name_ << std::endl;
}


void CsvExporter::ExportBatch(size_t batch_index) {
    try {
        for (size_t i = 0; i <= batch_index; i++) {
            ctp::ColumnarBatch batch = kio_reader_.ReadNextBatch();
            if (batch.empty()) {
                throw std::runtime_error("Batch is empty");
            }
            if (i == batch_index) {
                WriteBatchToStream(batch);
                break;
            }
        }
    } catch (const std::exception &e) {
        throw std::runtime_error("Error exporting batch " +
                                 std::to_string(batch_index) + ": " +
                                 e.what());
    }

    std::cout << "Successfully exported batch " << batch_index << " to "
            << csv_name_ << std::endl;
}


void CsvExporter::WriteBatchToStream(const ctp::ColumnarBatch &batch) {
    if (batch.empty()) {
        return;
    }

    const Schema& schema = kio_reader_.GetSchema();
    ctp::ValidateColumnarBatch(batch, schema);

    size_t num_rows = ctp::GetColumnRowCount(batch[0]);
    size_t num_cols = batch.size();
    const auto& column_types = schema.GetIndexToType();

    for (size_t row_idx = 0; row_idx < num_rows; ++row_idx) {
        for (size_t col_idx = 0; col_idx < num_cols; ++col_idx) {
            const bool should_quote = ShouldQuoteColumn(column_types[col_idx]);
            if (should_quote) {
                csv_file_ << '\"';
            }
            WriteColumnValue(batch[col_idx], row_idx, column_types[col_idx]);
            if (should_quote) {
                csv_file_ << '\"';
            }
            
            if (col_idx < num_cols - 1) {
                csv_file_ << ',';
            }
        }

        csv_file_ << "\n";
    }
}

void CsvExporter::WriteColumnValue(const ctp::Column& column, size_t row_idx, Schema::Types type) {
    switch (type) {
    case Schema::BIGINT:
        csv_file_ << ctp::GetColumnData<int64_t>(column)[row_idx];
        break;
    case Schema::INTEGER:
        csv_file_ << ctp::GetColumnData<int32_t>(column)[row_idx];
        break;
    case Schema::SMALLINT:
        csv_file_ << ctp::GetColumnData<int16_t>(column)[row_idx];
        break;
    case Schema::TEXT:
    case Schema::VARCHAR:
        WriteEscapedString(csv_file_, ctp::GetColumnData<std::string>(column)[row_idx]);
        break;
    case Schema::CHAR: {
        const char ch = ctp::GetColumnData<char>(column)[row_idx];
        if (ch == '"') {
            csv_file_ << "\"\"";
        } else {
            csv_file_ << ch;
        }
        break;
    }
    case Schema::TIMESTAMP:
        WriteEscapedString(csv_file_, FormatTimestamp(ctp::GetColumnData<int64_t>(column)[row_idx]));
        break;
    case Schema::DATE:
        WriteEscapedString(csv_file_, FormatDate(ctp::GetColumnData<int32_t>(column)[row_idx]));
        break;
    }
}
