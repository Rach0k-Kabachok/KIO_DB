#pragma once

#include <cstddef>
#include <fstream>
#include <string>

#include "columnar_types.h"
#include "schema.h"

class KioDbWriter {
public:
    KioDbWriter(const std::string& output_filename, const Schema& schema);

    void WriteBatchToFile(const ctp::ColumnarBatch& batch);

private:
    void WriteSchemaMeta();

    void WriteBatchMeta(const ctp::ColumnarBatch& batch);
    void WriteColumns(const ctp::ColumnarBatch& batch);

    std::ofstream kio_file_;
    std::string kio_name_;

    const Schema& schema_;

    size_t cur_batch_ = 0;
};
