#include "kio_work/kio_db_reader.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

#include "columnar_types.h"
#include "schema.h"

namespace {
void EnsureAvailable(const std::string& batch, size_t pos, uint64_t size) {
    if (pos > batch.size() || size > batch.size() - pos) {
        throw std::runtime_error("Corrupted KIO batch: column data is out of bounds");
    }
}

kio::ColumnChunkMeta ReadColumnChunkMeta(const std::string& batch, size_t& pos) {
    kio::ColumnChunkMeta chunk_meta{};

    EnsureAvailable(batch, pos, sizeof(chunk_meta));
    std::memcpy(&chunk_meta, batch.data() + pos, sizeof(chunk_meta));
    pos += sizeof(chunk_meta);

    return chunk_meta;
}
}  // namespace

KioDbReader::KioDbReader(const std::string& db_filename, const Schema& schema): 
    kio_file_(db_filename, std::ios::binary), 
    kio_name_(db_filename), 
    schema_(schema) {
    if (!kio_file_.is_open()) {
        throw std::runtime_error("Failed to open KIO file: " + db_filename);
    }
}

ctp::ColumnarBatch KioDbReader::ReadNextBatch() {
    kio::BatchMeta batch_meta{};
    kio_file_.read(reinterpret_cast<char*>(&batch_meta), sizeof(batch_meta));
    if (!kio_file_) {
        if (kio_file_.eof() && kio_file_.gcount() == 0) {
            return {};
        }
        throw std::runtime_error("Failed to read batch metadata");
    }

    ctp::ColumnarBatch batch;

    std::string readed_batch(batch_meta.batch_size, '\0');
    kio_file_.read(readed_batch.data(), static_cast<std::streamsize>(readed_batch.size()));
    if (!kio_file_) {
        throw std::runtime_error("Failed to read batch data");
    }

    size_t pos = 0;
    for (size_t col = 0; col < batch_meta.col_num; col++) {
        kio::ColumnChunkMeta chunk_meta = ReadColumnChunkMeta(readed_batch, pos);

        switch (chunk_meta.type) {
            case Schema::INT64:
                batch.emplace_back(ReadNumColumn(pos, batch_meta.row_num, chunk_meta, readed_batch));
                break;
            case Schema::STRING:
                batch.emplace_back(ReadStrColumn(pos, batch_meta.row_num, chunk_meta, readed_batch));
                break;
            default:
                throw std::runtime_error("Unsupported column type in batch import");
        }
    }

    if (pos != readed_batch.size()) {
        throw std::runtime_error("Corrupted KIO batch: payload size mismatch");
    }

    return batch;
}

ctp::Column KioDbReader::ReadStrColumn(size_t& pos, uint64_t row_num,
                                      const kio::ColumnChunkMeta& chunk_meta,
                                      const std::string& batch) {
    std::vector<std::string> result;
    result.reserve(row_num);

    std::vector<uint64_t> str_sizes(row_num);
    uint64_t sizes_bytes = row_num * sizeof(uint64_t);
    EnsureAvailable(batch, pos, sizes_bytes);
    std::memcpy(str_sizes.data(), batch.data() + pos, sizes_bytes);
    pos += sizes_bytes;

    uint64_t strings_size = 0;
    for (const auto& sz : str_sizes) {
        strings_size += sz;
        EnsureAvailable(batch, pos, sz);
        result.emplace_back(batch.data() + pos, sz);
        pos += sz;
    }
    if (strings_size != chunk_meta.size) {
        throw std::runtime_error("Corrupted KIO batch: string column size mismatch");
    }

    return result;
}

ctp::Column KioDbReader::ReadNumColumn(size_t& pos, uint64_t row_num,
                                      const kio::ColumnChunkMeta& chunk_meta,
                                      const std::string& batch) {
    uint64_t expected_size = row_num * sizeof(int64_t);
    if (chunk_meta.size != expected_size) {
        throw std::runtime_error("Corrupted KIO batch: numeric column size mismatch");
    }

    std::vector<int64_t> result(row_num);
    EnsureAvailable(batch, pos, expected_size);
    std::memcpy(result.data(), batch.data() + pos, expected_size);
    pos += expected_size;
    return result;
}
