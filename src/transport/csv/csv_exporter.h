#pragma once

#include <string>
#include <fstream>

#include "columnar_types.h"
#include "transport/kio/kio_db_reader.h"
#include "schema.h"

class CsvExporter {
public:
    CsvExporter(KioDbReader& reader, const std::string& csv_filename);
    

    void Export();

private:
    void WriteBatchToStream(const KioReadBatch& batch);
    
    void WriteColumnValue(const ctp::Column& column, size_t row_idx, Schema::Types type);
    
    std::ofstream csv_file_;
    KioDbReader& kio_reader_;
};
