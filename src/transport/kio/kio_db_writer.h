#pragma once

#include <cstddef>
#include <fstream>
#include <string>

#include "global/columnar_types.h"
#include "global/schema.h"
#include "transport/kio/kio_format.h"

class KioDbWriter {
public:
    KioDbWriter(const std::string& output_filename, const Schema& schema);
    ~KioDbWriter();

    void WriteBatchToFile(const ctp::ColumnarBatch& batch);
    void Finalize();

private:
    void WriteHeader();
    void WriteFooter();

    kio::BatchMeta MakeBatchMeta(const ctp::ColumnarBatch& batch);
    std::vector<kio::ColumnChunkInfo> WriteColumns(
        const ctp::ColumnarBatch& batch);
    
    void WriteSchema();
    void WriteColumnChunkInfo(const kio::ColumnChunkInfo& chunk);
    void WriteRowGroupMeta(const kio::RowGroupMeta& row_group);

    std::ofstream kio_file_;

    kio::FileMetadata metadata_;

    size_t cur_batch_ = 0;
    bool finalized_ = false;
};
