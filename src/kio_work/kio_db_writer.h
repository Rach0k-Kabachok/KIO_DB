#pragma once

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

#include "columnar_types.h"
#include "schema.h"

class KioDbWriter {
    // Сигнатура конца батча (помогает найти и проверить BatchMeta при чтении)
    static constexpr std::uint32_t kBatchMetaMagic = 0x4B494F4D;  // "KIOM"

    std::string db_filename_;
    std::ofstream db_file_;
    bool is_finished_ = false;
    std::uint64_t next_batch_id_ = 0;

public:
    explicit KioDbWriter(const std::string &output_db_name);

    KioDbWriter(const KioDbWriter &) = delete;
    KioDbWriter &operator=(const KioDbWriter &) = delete;

    KioDbWriter(KioDbWriter &&) = default;
    KioDbWriter &operator=(KioDbWriter &&) = default;

    ~KioDbWriter();

    void WriteBatch(const ctp::ColumnarBatch &batch, const Schema &schema);

    void Finish();

private:
    void WriteBatchMetaBlock(const kio::BatchMeta &meta);

    void WriteVectorToStream(const std::vector<std::int64_t> &vec,
                             kio::ColumnChunkMeta &meta);

    void WriteVectorToStream(const std::vector<std::string> &vec,
                             kio::ColumnChunkMeta &meta);
};
