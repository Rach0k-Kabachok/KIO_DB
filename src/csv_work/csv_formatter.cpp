#include "csv_work/csv_formatter.h"

#include <vector>
#include <string>
#include <iostream>

#include "columnar_types.h"
#include "csv_work/csv_batch_reader.h"
#include "schema.h"

CSVFormatter::CSVFormatter(const std::string &data_name, const Schema &schema)
        : batch_reader_(data_name), schema_(schema) {
}

ctp::ColumnarBatch CSVFormatter::MakeColumnarBatch() {
    ctp::ParsedBatch batch = batch_reader_.ParseNextBatch();

    if (batch.empty()) {
        return {};
    }

    size_t num_rows = batch.size();
    size_t num_cols = batch[0].size();

    ctp::ColumnarBatch result;
    result.reserve(num_cols);

    for (size_t col_idx = 0; col_idx < num_cols; ++col_idx) {
        auto type = schema_.SearchTypeByIndex(col_idx);

        switch (type) {
        case Schema::INT64: {
            std::vector<int64_t> int_vec;
            int_vec.reserve(num_rows);

            for (size_t row_idx = 0; row_idx < num_rows; ++row_idx) {
                try {
                    const std::string &num = batch[row_idx][col_idx];
                    if (num.empty()) {
                        std::cerr << "Warning: Empty value at row " << row_idx << " col "
                                  << col_idx << ", treating as 0\n";
                        int_vec.push_back(0);
                    } else {
                        int_vec.push_back(std::stoll(num));
                    }
                } catch (const std::exception &e) {
                    std::cerr << "Parse error at row " << row_idx << " col "
                              << col_idx << ": " << e.what() << "\n";
                    int_vec.push_back(0);
                }
            }
            result.emplace_back(std::move(int_vec));
            break;
        }
        case Schema::STRING: {
            std::vector<std::string> str_vec;
            str_vec.reserve(num_rows);

            for (size_t row_idx = 0; row_idx < num_rows; ++row_idx) {
                str_vec.push_back(batch[row_idx][col_idx]);
            }
            result.emplace_back(std::move(str_vec));
            break;
        }
        }
    }

    return result;
}
