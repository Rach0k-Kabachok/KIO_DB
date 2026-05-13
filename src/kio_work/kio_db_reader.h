#pragma once

#include <cstddef>
#include <fstream>
#include <string>
#include <cstdint>

#include "columnar_types.h"
#include "schema.h"

class KioDbReader {

public:
    KioDbReader(const std::string& db_filename, const Schema& schema);

    ctp::ColumnarBatch ReadNextBatch();

private:
    ctp::Column ReadStrColumn(size_t& pos, uint64_t row_num,
                              const kio::ColumnChunkMeta& chunk_meta,
                              const std::string& batch);
    ctp::Column ReadNumColumn(size_t& pos, uint64_t row_num, 
                              const kio::ColumnChunkMeta& chunk_meta,
                              const std::string& batch);

    std::ifstream kio_file_;
    std::string kio_name_;
  
    const Schema& schema_;

};
