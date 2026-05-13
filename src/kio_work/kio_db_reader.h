#pragma once

#include <cstddef>
#include <fstream>
#include <string>
#include <cstdint>
#include <vector>

#include "columnar_types.h"
#include "schema.h"

class KioDbReader {

public:
    explicit KioDbReader(const std::string& db_filename);

    const Schema& GetSchema() const;

    const std::vector<std::string>& GetColumnNames() const;
    const std::vector<Schema::Types>& GetColumnTypes() const;

    void LoadSchema(Schema& schema) const;

    void Reset();

    ctp::ColumnarBatch ReadNextBatch();

private:
    void ReadSchemaMeta();

    std::ifstream kio_file_;
    std::streampos batches_start_pos_ = 0;
    Schema schema_;

};
