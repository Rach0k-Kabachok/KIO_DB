#pragma once

#include <string>

#include "kio_work/kio_db_reader.h"

class CsvExporter {
public:
    static void Export(KioDbReader &reader, const std::string &out_filename);

    static void ExportBatch(KioDbReader &reader, size_t batch_index,
                            const std::string &out_filename);

private:
    static void WriteBatchToStream(const ctp::ColumnarBatch &batch,
                                   std::ostream &out);
};
