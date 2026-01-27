#pragma once

#include <vector>
#include <string>
#include <variant>
#include <iostream>

#include "csv_batch_reader.h"
#include "schema.h"

class CSVFormatter {
public:
    using Column = std::variant<std::vector<int64_t>, std::vector<std::string>>;
    using ColumnarBatch = std::vector<Column>;

private:
    using ParsedBatch = std::vector<std::vector<std::string>>;

    CSVBatchReader batch_reader_;
    Schema schema_;

public:
    CSVFormatter(const std::string& data_name, const std::string& schema_name)
        : batch_reader_(data_name) {

        CSVBatchReader schema_reader(schema_name);
        ParsedBatch batch;
        while (!(batch = schema_reader.ParseNextBatch()).empty()) {
            schema_.ImplSchema(batch);
        }
    }

    ColumnarBatch MakeColumnarBatch() {
        ParsedBatch batch = batch_reader_.ParseNextBatch();

        if (batch.empty()) {
            return {};
        }

        size_t num_rows = batch.size();
        size_t num_cols = batch[0].size();

        ColumnarBatch result;
        result.reserve(num_cols);

        for (size_t col_idx = 0; col_idx < num_cols; ++col_idx) {

            if (col_idx >= num_cols) break;

            auto type = schema_.SearchTypeByIndex(col_idx);

            if (type == Schema::INT64) {
                std::vector<int64_t> int_vec;
                int_vec.reserve(num_rows);

                for (size_t row_idx = 0; row_idx < num_rows; ++row_idx) {
                    try {
                        const std::string& val_str = batch[row_idx][col_idx];
                        if (val_str.empty()) {
                            int_vec.push_back(0);
                        } else {
                            int_vec.push_back(std::stoll(val_str));
                        }
                    } catch (const std::exception& e) {
                        std::cerr << "Parse error at row " << row_idx << " col " << col_idx << ": " << e.what() << "\n";
                        int_vec.push_back(0);
                    }
                }
                result.emplace_back(std::move(int_vec));

            } else if (type == Schema::STRING) {
                std::vector<std::string> str_vec;
                str_vec.reserve(num_rows);

                for (size_t row_idx = 0; row_idx < num_rows; ++row_idx) {
                    str_vec.push_back(batch[row_idx][col_idx]);
                }
                result.emplace_back(std::move(str_vec));
            }
        }

        return result;
    }

    // Константный доступ, если не планируем менять схему снаружи
    const Schema& GetSchema() const {
        return schema_;
    }

    Schema GetSchema() {
        return schema_;
    }
};
