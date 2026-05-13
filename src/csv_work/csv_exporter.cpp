#include "csv_work/csv_exporter.h"

#include <string>
#include <fstream>
#include <vector>
#include <variant>
#include <iostream>
#include <stdexcept>

#include "kio_work/kio_db_reader.h"

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

    size_t num_rows =
            std::visit([](const auto &v) { return v.size(); }, batch[0]);
    size_t num_cols = batch.size();

    for (size_t col_idx = 0; col_idx < num_cols; ++col_idx) {
        size_t col_size = std::visit([](const auto &v) { return v.size(); },
                                     batch[col_idx]);
        if (col_size != num_rows) {
            throw std::runtime_error("Column " + std::to_string(col_idx) +
                                     " has " + std::to_string(col_size) +
                                     " rows, expected " +
                                     std::to_string(num_rows));
        }
    }

    for (size_t row_idx = 0; row_idx < num_rows; ++row_idx) {
        for (size_t col_idx = 0; col_idx < num_cols; ++col_idx) {
            csv_file_ << '\"';
            const auto &column_variant = batch[col_idx];

            std::visit(
                [this, row_idx](const auto &vec) {
                    using T = std::decay_t<decltype(vec)>;
                    using Elem = T::value_type;

                    if constexpr (std::is_same_v<Elem, std::string>) {
                        const std::string &s = vec[row_idx];
                        for (char ch: s) {
                            if (ch == '"') {
                                csv_file_ << "\"\"";
                            } else {
                                csv_file_ << ch;
                            }
                        }
                    } else if constexpr (std::is_same_v<Elem, std::int64_t>) {
                        csv_file_ << vec[row_idx];
                    } else {
                        throw std::runtime_error(
                            "Unsupported column type in batch export");
                    }
                },
                column_variant);

            csv_file_ << '\"';
            if (col_idx < num_cols - 1) {
                csv_file_ << ',';
            }
        }

        csv_file_ << "\n";
    }
}
