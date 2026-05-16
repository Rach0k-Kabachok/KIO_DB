#include "transport/csv/csv_exporter.h"

#include <cstdint>
#include <iomanip>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>

#include "global/column_operations.h"
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
        : kio_reader_(reader) {
    csv_file_.open(csv_filename, std::ios::out);
    if (!csv_file_.is_open()) {
        throw std::runtime_error("Cannot open CSV file for writing: " +
                                 csv_filename);
    }
}

void CsvExporter::Export() {
    while (std::optional<KioReadBatch> batch = kio_reader_.ReadNextBatch()) {
        WriteBatchToStream(*batch);
    }
}


void CsvExporter::WriteBatchToStream(const KioReadBatch& batch) {
    if (batch.columns.empty()) {
        return;
    }

    const Schema& schema = kio_reader_.GetSchema();
    const size_t num_rows = batch.row_count;
    const size_t num_cols = batch.columns.size();

    for (size_t row_idx = 0; row_idx < num_rows; ++row_idx) {
        for (size_t col_idx = 0; col_idx < num_cols; ++col_idx) {
            const Schema::Types column_type = schema.ColumnType(col_idx);
            const bool should_quote = ShouldQuoteColumn(column_type);
            if (should_quote) {
                csv_file_ << '\"';
            }
            WriteColumnValue(batch.columns[col_idx], row_idx, column_type);
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

void CsvExporter::WriteColumnValue(const ctp::Column& column, size_t row_idx,
                                   Schema::Types type) {
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
        WriteEscapedString(csv_file_,
                           ctp::GetColumnData<std::string>(column)[row_idx]);
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
        WriteEscapedString(
            csv_file_,
            FormatTimestamp(ctp::GetColumnData<int64_t>(column)[row_idx]));
        break;
    case Schema::DATE:
        WriteEscapedString(
            csv_file_,
            FormatDate(ctp::GetColumnData<int32_t>(column)[row_idx]));
        break;
    }
}
