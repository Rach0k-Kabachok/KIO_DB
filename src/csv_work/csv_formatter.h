#pragma once

#include <string>

#include "columnar_types.h"
#include "csv_work/csv_batch_reader.h"
#include "schema.h"

class CSVFormatter : public Schema {
    CSVBatchReader batch_reader_;

public:
    CSVFormatter(const std::string &data_name, const std::string &schema_name);

    ctp::ColumnarBatch MakeColumnarBatch();
};
