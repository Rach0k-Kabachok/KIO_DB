#pragma once

#include <cstddef>
#include <string>
#include <string_view>

#include "columnar_types.h"
#include "schema.h"
#include "transport/csv/csv_batch_reader.h"

class CSVColumnarReader {
public:
    CSVColumnarReader(const std::string& data_name, const Schema& schema);

    ctp::ColumnarBatch MakeColumnarBatch();

private:
    ctp::ColumnarBatch MakeEmptyBatch(size_t reserve_rows) const;

    void ParseBuffer(std::string&& buffer, ctp::ColumnarBatch& batch) const;

    void AppendField(ctp::ColumnarBatch& batch, size_t col_idx,
                     std::string_view field) const;

    CSVBatchReader batch_reader_;
    const Schema& schema_;
};
