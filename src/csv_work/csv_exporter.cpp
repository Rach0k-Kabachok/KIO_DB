#include "csv_work/csv_exporter.h"

#include <chrono>
#include <iomanip>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <vector>

#include "columnar_batch.h"
#include "column_operations.h"
#include "kio_work/kio_db_reader.h"

namespace {
void WriteEscapedString(std::ofstream& csv_file, const std::string& value) {
    for (char ch : value) {
        if (ch == '"') {
            csv_file << "\"\"";
        } else {
            csv_file << ch;
        }
    }
}

std::string FormatDate(std::int32_t days) {
    const std::chrono::sys_days date{std::chrono::days{days}};
    const std::chrono::year_month_day ymd{date};

    std::ostringstream result;
    result << std::setfill('0') << std::setw(4)
           << static_cast<int>(ymd.year()) << '-'
           << std::setw(2) << static_cast<unsigned>(ymd.month()) << '-'
           << std::setw(2) << static_cast<unsigned>(ymd.day());
    return result.str();
}

std::string FormatTimestamp(std::int64_t seconds) {
    const std::chrono::sys_seconds time{std::chrono::seconds{seconds}};
    const auto date = std::chrono::floor<std::chrono::days>(time);
    const std::chrono::year_month_day ymd{date};
    const std::chrono::hh_mm_ss hms{time - date};

    std::ostringstream result;
    result << std::setfill('0') << std::setw(4)
           << static_cast<int>(ymd.year()) << '-'
           << std::setw(2) << static_cast<unsigned>(ymd.month()) << '-'
           << std::setw(2) << static_cast<unsigned>(ymd.day()) << ' '
           << std::setw(2) << hms.hours().count() << ':'
           << std::setw(2) << hms.minutes().count() << ':'
           << std::setw(2) << hms.seconds().count();
    return result.str();
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
            csv_file_ << '\"';
            WriteColumnValue(batch[col_idx], row_idx, column_types[col_idx]);
            csv_file_ << '\"';
            
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
