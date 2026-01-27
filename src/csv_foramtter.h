#pragma once

#include <vector>
#include <string>
#include <variant>
#include <memory>

#include <csv_batch_reader.h>
#include <schema.h>

class CSVFormatter {
    using Column = std::variant<std::vector<int64_t>, std::vector<std::string>>;
    using ColumnarBatch = std::vector<Column>;
    using ParsedBatch = std::vector<std::vector<std::string> >;
    using ParsedRow = std::vector<std::string>;

    CSVBatchReader batch_reader_;

    Schema schema_;

public:
    CSVFormatter(const std::string &data_name, const std::string &schema_name)
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

        size_t col_num = batch.size();
        size_t row_num = batch[0].size();
    
        ColumnarBatch result;
        result.reserve(row_num);

        for (size_t col_idx = 0; col_idx < row_num; ++col_idx) {

            auto type = schema_.SearchTypeByIndex(col_idx);

            if (type == Schema::INT64) {
                std::vector<int64_t> int_vec;
                int_vec.reserve(col_num);

                for (size_t row_idx = 0; row_idx < col_num; ++row_idx) {
                    int_vec.push_back(std::stoll(batch[row_idx][col_idx]));
                }
                result.emplace_back(std::move(int_vec));

            } else if (type == Schema::STRING) {
                std::vector<std::string> str_vec;
                str_vec.reserve(col_num);

                for (size_t row_idx = 0; row_idx < col_num; ++row_idx) {
                    str_vec.push_back(batch[row_idx][col_idx]);
                }
                result.emplace_back(std::move(str_vec));
            }
        }

        return result;
    }

    Schema &GetSchema() {
        return schema_;
    }
};
