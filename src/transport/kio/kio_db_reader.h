#pragma once

#include <fstream>
#include <optional>
#include <string>
#include <vector>

#include "global/columnar_types.h"
#include "global/schema.h"
#include "transport/kio/kio_format.h"

struct KioReadBatch {
    ctp::ColumnarBatch columns;
    uint64_t row_count = 0;
};

class KioDbReader {
public:
    explicit KioDbReader(const std::string& db_filename);

    const Schema& GetSchema() const;
    const kio::FileMetadata& GetMetadata() const;
    const kio::RowGroupMeta& GetRowGroupMeta(size_t group_idx) const;
    size_t GetNextRowGroupIndex() const;

    std::optional<KioReadBatch> ReadNextBatch();

    std::optional<KioReadBatch> ReadNextBatch(
        const std::vector<size_t>& column_indices);

    void SkipNextBatch();

private:
    void Reset();
    void ReadHeader();
    void ReadFooter(uint64_t footer_offset);

    std::ifstream kio_file_;
    kio::FileMetadata metadata_;
    size_t next_row_group_ = 0;
};
