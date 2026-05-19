#pragma once

#include <fstream>
#include <optional>
#include <string>
#include <vector>

#include "global/columnar_types.h"
#include "global/schema.h"
#include "transport/kio/kio_format.h"

class KioDbReader {
public:
    explicit KioDbReader(const std::string& db_filename);

    const Schema& GetSchema() const;
    const kio::FileMetadata& GetMetadata() const;
    const kio::RowGroupMeta& GetRowGroupMeta() const;
    const kio::RowGroupMeta* PeekNextRowGroup() const;

    std::optional<ctp::ColumnarBatch> ReadNextBatch();

    std::optional<ctp::ColumnarBatch> ReadNextBatch(
        const std::vector<size_t>& column_indices);

    void SkipNextBatch();

private:
    void Reset();
    void ReadHeader();
    void ReadFooter(uint64_t footer_offset);

    Schema ReadSchema();
    kio::ColumnChunkInfo ReadColumnChunkInfo();
    kio::RowGroupMeta ReadRowGroupMeta();
    ctp::Column ReadColumnFromFile(uint64_t row_num,
                                   Schema::Types type,
                                   const kio::ColumnChunkInfo& chunk);

    std::ifstream kio_file_;
    kio::FileMetadata metadata_;
    size_t next_row_group_ = 0;
};
