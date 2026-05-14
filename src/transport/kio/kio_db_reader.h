#pragma once

#include <fstream>
#include <string>
#include <vector>

#include "columnar_types.h"
#include "schema.h"

class KioDbReader {

public:
    KioDbReader(const std::string& db_filename, const Schema& schema);

    const Schema& GetSchema() const;

    const std::vector<std::string>& GetColumnNames() const;
    const std::vector<Schema::Types>& GetColumnTypes() const;

    void Reset();

    ctp::ColumnarBatch ReadNextBatch();

    ctp::ColumnarBatch ReadNextProjectedBatch(
        const std::vector<size_t>& column_indices);

private:
    void ReadSchemaMeta();

    std::ifstream kio_file_;
    std::streampos batches_start_pos_ = 0;
    const Schema& schema_;

};
