#pragma once

#include <string>
#include <fstream>

#include "kio_work/kio_db_reader.h"

class CsvExporter {
public:
    CsvExporter(KioDbReader& reader, const std::string& csv_filename);
    

    void Export();

    void ExportBatch(size_t batch_index);

private:
    void WriteBatchToStream(const ctp::ColumnarBatch &batch);
    
    
    std::ofstream csv_file_;
    std::string csv_name_;
    KioDbReader& kio_reader_;
};
