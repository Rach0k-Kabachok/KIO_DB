#pragma once

#include <cstddef>
#include <fstream>
#include <string>

#include "columnar_types.h"
#include "transport/kio/kio_db_reader.h"
#include "schema.h"

class CsvExporter {
public:
    CsvExporter(const std::string& csv_filename);
    

    void ExportFile(KioDbReader& reader);
    void ExportBatch(const Schema& schema, const ctp::ColumnarBatch& columns,
                     size_t row_count);

private:
    void WriteBatchToStream(const Schema& schema,
                            const ctp::ColumnarBatch& columns,
                            size_t row_count);
    
    void WriteColumnValue(const ctp::Column& column, size_t row_idx, Schema::Types type);
    
    std::ofstream csv_file_;
};
