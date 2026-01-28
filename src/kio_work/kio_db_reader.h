#pragma once

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

#include "columnar_types.h"

class KioDbReader {
    // Должно совпадать с writer'ом (сигнатура конца батча)
    static constexpr std::uint32_t kBatchMetaMagic = 0x4B494F4D; // "KIOM"
    static constexpr std::uint64_t kTrailerSize =
            sizeof(std::uint64_t) + sizeof(std::uint32_t); // meta_size + magic

    std::string filename_;
    std::ifstream file_;
    std::vector<kio::BatchMeta> meta_index_;

public:
    explicit KioDbReader(const std::string &filename);

    KioDbReader(const KioDbReader &) = delete;

    KioDbReader &operator=(const KioDbReader &) = delete;

    KioDbReader(KioDbReader &&) = default;

    KioDbReader &operator=(KioDbReader &&) = default;

    ~KioDbReader();

    size_t GetBatchCount() const;

    ctp::ColumnarBatch ReadBatch(size_t batch_index);

private:
    void BuildIndexFromEnd();

    std::vector<std::int64_t> ReadIntColumn(const kio::ColumnChunkMeta &meta);

    std::vector<std::string> ReadStringColumn(
        const kio::ColumnChunkMeta &meta);
};
