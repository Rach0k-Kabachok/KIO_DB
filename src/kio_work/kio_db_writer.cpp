#include "kio_work/kio_db_writer.h"

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

#include "columnar_types.h"
#include "schema.h"

namespace {
auto MakeColumnChunkMeta(Schema::Types type, uint64_t size) {
    kio::ColumnChunkMeta chunk_meta{};
    chunk_meta.type = static_cast<uint8_t>(type);
    chunk_meta.size = size;
    return chunk_meta;
}

void WriteColumnChunkMeta(std::ofstream& kio_file, const kio::ColumnChunkMeta& chunk_meta) {
    kio_file.write(reinterpret_cast<const char*>(&chunk_meta), sizeof(chunk_meta));
}

std::vector<uint64_t> GetStringSizes(const std::vector<std::string>& strings) {
    std::vector<uint64_t> sizes;
    sizes.reserve(strings.size());
    for (const auto& str : strings) {
        sizes.emplace_back(str.size());
    }
    return sizes;
}
    
size_t CountWrittenBatchSize(const ctp::ColumnarBatch& batch) {
    size_t result = 0;
    for (size_t i = 0; i < batch.size(); i++) {
        result += sizeof(kio::ColumnChunkMeta);

        std::visit([&result](const auto& vec){
            using T = std::decay_t<decltype(vec)>;
            using Elem = T::value_type;

            if constexpr (std::is_same_v<Elem, std::string>) {
                std::vector<uint64_t> str_sizes = GetStringSizes(vec);
                for (size_t j = 0; j < str_sizes.size(); j++) {
                    result += sizeof(uint64_t) + str_sizes[j];
                }
            } else if constexpr (std::is_same_v<Elem, int64_t>) {
                result += sizeof(int64_t) * vec.size();
            } else {
                throw std::runtime_error(
                            "Unsupported column type in batch export");
            }
        }, batch[i]);
    }

    return result;
}
} // namespace

KioDbWriter::KioDbWriter(const std::string& output_filename, const Schema& schema)
    : kio_name_(output_filename), schema_(schema) {

    kio_file_.open(kio_name_, std::ios::binary | std::ios::out);
    if (!kio_file_.is_open()) {
        throw std::runtime_error("Failed to open file: " + kio_name_);
    }
}

void KioDbWriter::WriteBatchMeta(const ctp::ColumnarBatch& batch) {
    if (batch.empty()) {
        return;
    } 
    uint64_t batch_id = cur_batch_++;
    uint64_t row_num = std::visit([](const auto& vec) { return vec.size();}, batch[0]);
    uint64_t col_num = batch.size();
    uint64_t batch_start_offset = static_cast<uint64_t>(kio_file_.tellp()) 
                                  + sizeof(uint64_t) * 5;  // 5 metadata fields
    uint64_t batch_size = CountWrittenBatchSize(batch);

    kio_file_.write(reinterpret_cast<const char*>(&batch_id), sizeof(batch_id));
    kio_file_.write(reinterpret_cast<const char*>(&row_num), sizeof(row_num));
    kio_file_.write(reinterpret_cast<const char*>(&col_num), sizeof(col_num));
    kio_file_.write(reinterpret_cast<const char*>(&batch_start_offset), sizeof(batch_start_offset));
    kio_file_.write(reinterpret_cast<const char*>(&batch_size),sizeof(batch_size));

    if (!kio_file_.good()) {
        throw std::runtime_error("Failed to write batch metadata");
    }
}

void KioDbWriter::WriteColumns(const ctp::ColumnarBatch& batch) {
    for (size_t i = 0; i < batch.size(); i++) {
        std::visit([this](const auto& vec) {
        WriteVectorToFile(vec);
        }, batch[i]);
    }
}

template<typename T>
void KioDbWriter::WriteVectorToFile(const std::vector<T>& nums) {
    static_assert(std::is_arithmetic_v<T>, "Only numeric vectors are supported");

    kio::ColumnChunkMeta chunk_meta = MakeColumnChunkMeta(Schema::INT64, nums.size() * sizeof(T));
    WriteColumnChunkMeta(kio_file_, chunk_meta);
    kio_file_.write(reinterpret_cast<const char*>(nums.data()), chunk_meta.size);

    if (!kio_file_.good()) {
        throw std::runtime_error("Failed to write numeric column");
    }
}

void KioDbWriter::WriteVectorToFile(const std::vector<std::string>& strings) {
    std::vector<uint64_t> str_sizes = GetStringSizes(strings);
    uint64_t size = 0;
    for (const auto& sz : str_sizes) {
        size += sz;
    }

    kio::ColumnChunkMeta chunk_meta = MakeColumnChunkMeta(Schema::STRING, size);
    WriteColumnChunkMeta(kio_file_, chunk_meta);
    kio_file_.write(reinterpret_cast<const char*>(str_sizes.data()),
                               sizeof(uint64_t) * str_sizes.size());

    for (const auto& str : strings) {
        kio_file_.write(str.data(), str.size());
    }

    if (!kio_file_.good()) {
        throw std::runtime_error("Failed to write string column");
    }
}

void KioDbWriter::WriteBatchToFile(const ctp::ColumnarBatch& batch) {
    if (batch.empty()) {
        return;
    }

    WriteBatchMeta(batch);
    WriteColumns(batch);
}
