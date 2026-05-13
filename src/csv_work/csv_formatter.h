#pragma once

#include <string>

#include "columnar_types.h"
#include "csv_work/csv_batch_reader.h"
#include "schema.h"

class CSVFormatter {
public:
    CSVFormatter(const std::string &data_name, const Schema &schema);

    ctp::ColumnarBatch MakeColumnarBatch();

    static size_t CountTime(std::string time);
    
private:
    CSVBatchReader batch_reader_;
    const Schema &schema_;
};
