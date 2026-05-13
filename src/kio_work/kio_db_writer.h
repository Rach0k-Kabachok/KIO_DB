#pragma once

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

#include "columnar_types.h"
#include "schema.h"

class KioDbWriter {
public:
    KioDbWriter(const std::string& output_filename, const Schema& schema);

    void WriteBatchToFile(const ctp::ColumnarBatch& batch);

private:
    void WriteBatchMeta(const ctp::ColumnarBatch& batch);
    void WriteColumns(const ctp::ColumnarBatch& batch);


    template<typename T>
    //write column of nums
    void WriteVectorToFile(const std::vector<T>& nums);
    //write column of strings
    void WriteVectorToFile(const std::vector<std::string>& strings);

    std::ofstream kio_file_;
    std::string kio_name_;

    const Schema& schema_;

    size_t cur_batch_ = 0;
};
