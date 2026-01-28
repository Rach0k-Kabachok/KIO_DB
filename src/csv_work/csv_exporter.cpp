#include "csv_work/csv_exporter.h"

#include <string>
#include <fstream>
#include <vector>
#include <variant>
#include <iostream>
#include <stdexcept>

#include "kio_work/kio_db_reader.h"


void CsvExporter::Export(KioDbReader &reader, const std::string &out_filename) {
    std::ofstream out(out_filename);
    if (!out.is_open()) {
        throw std::runtime_error("Cannot open output CSV file: " +
                                 out_filename);
    }

    size_t batch_count = reader.GetBatchCount();

    if (batch_count == 0) {
        std::cerr << "No batches to export" << std::endl;
        return;
    }

    for (size_t i = 0; i < batch_count; ++i) {
        try {
            auto batch = reader.ReadBatch(i);
            if (batch.empty()) {
                std::cerr << "Warning: Batch " << i << " is empty, skipping"
                        << std::endl;
                continue;
            }

            WriteBatchToStream(batch, out);
        } catch (const std::exception &e) {
            throw std::runtime_error("Error exporting batch " +
                                     std::to_string(i) + ": " + e.what());
        }
    }

    out.close();
    if (!out.good()) {
        throw std::runtime_error("Error writing to output CSV file");
    }

    std::cout << "Successfully exported " << batch_count << " batches to "
            << out_filename << std::endl;
}


void CsvExporter::ExportBatch(KioDbReader &reader, size_t batch_index,
                              const std::string &out_filename) {
    std::ofstream out(out_filename);
    if (!out.is_open()) {
        throw std::runtime_error("Cannot open output CSV file: " +
                                 out_filename);
    }

    try {
        auto batch = reader.ReadBatch(batch_index);
        if (batch.empty()) {
            throw std::runtime_error("Batch is empty");
        }

        WriteBatchToStream(batch, out);
    } catch (const std::exception &e) {
        throw std::runtime_error("Error exporting batch " +
                                 std::to_string(batch_index) + ": " +
                                 e.what());
    }

    out.close();
    if (!out.good()) {
        throw std::runtime_error("Error writing to output CSV file");
    }

    std::cout << "Successfully exported batch " << batch_index << " to "
            << out_filename << std::endl;
}


void CsvExporter::WriteBatchToStream(const ctp::ColumnarBatch &batch,
                                     std::ostream &out) {
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
            out << "\"";
            const auto &column_variant = batch[col_idx];

            std::visit(
                [&out, row_idx](const auto &vec) {
                    using T = std::decay_t<decltype(vec)>;
                    using Elem = T::value_type;

                    if constexpr (std::is_same_v<Elem, std::string>) {
                        const std::string &s = vec[row_idx];
                        for (char ch: s) {
                            if (ch == '"') {
                                out << "\"\"";
                            } else {
                                out << ch;
                            }
                        }
                    } else {
                        out << vec[row_idx];
                    }
                },
                column_variant);

            out << "\"";
            if (col_idx < num_cols - 1) {
                out << ",";
            }
        }

        out << "\n";
    }
}
