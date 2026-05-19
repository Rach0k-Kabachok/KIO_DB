#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "gtest/gtest.h"

#include "columnar_types.h"
#include "transport/kio/kio_db_importer.h"
#include "transport/kio/kio_db_reader.h"
#include "transport/kio/kio_db_writer.h"
#include "transport/csv/csv_exporter.h"

#include "transport/csv/csv_batch_reader.h"
#include "schema.h"

std::filesystem::path ReadWriteTestDataDir() {
    return std::filesystem::path(__FILE__).parent_path();
}

std::filesystem::path ReadWriteTestOutputPath(const std::string& filename) {
    return std::filesystem::temp_directory_path() / filename;
}

Schema LoadReadWriteTestSchema(const std::string& path_schema) {
    CSVBatchReader schema_reader(path_schema);
    ctp::ParsedBatch schema_rows;
    ctp::ParsedBatch batch;

    while (!(batch = schema_reader.ParseNextBatch()).empty()) {
        schema_rows.insert(schema_rows.end(), batch.begin(), batch.end());
    }

    return Schema(schema_rows);
}

TEST(ReadWrite, JustWorks) {
    const std::string path_csv =
        (ReadWriteTestDataDir() / "test_csv1.csv").string();
    const std::string path_schema =
        (ReadWriteTestDataDir() / "test_schema1.csv").string();

    const std::string path_db =
        ReadWriteTestOutputPath("kio_db_read_write_test.kiodb").string();

    const std::string path_out_csv =
        ReadWriteTestOutputPath("kio_db_read_write_test.csv").string();

    std::error_code ec;
    std::filesystem::remove(path_db, ec);
    std::filesystem::remove(path_out_csv, ec);

    {
        Schema schema = LoadReadWriteTestSchema(path_schema);
        KioDbWriter writer(path_db, schema);
        KioDbImporter importer(path_csv, schema, writer);
        ASSERT_NO_THROW(importer.Import());
    }

    ASSERT_TRUE(std::filesystem::exists(path_db));
    ASSERT_GT(std::filesystem::file_size(path_db), 0u);

    {
        KioDbReader reader(path_db);
        ASSERT_NO_THROW({
            CsvExporter exporter(path_out_csv);
            exporter.ExportFile(reader);
        });
    }

    ASSERT_TRUE(std::filesystem::exists(path_out_csv));
    ASSERT_GT(std::filesystem::file_size(path_out_csv), 0u);

    CSVBatchReader orig_reader(path_csv);
    CSVBatchReader copy_reader(path_out_csv);

    std::vector<std::vector<std::string> > orig_parsed_batch;

    while (!(orig_parsed_batch = orig_reader.ParseNextBatch()).empty()) {
        EXPECT_EQ(orig_parsed_batch, copy_reader.ParseNextBatch());
        orig_parsed_batch.clear();
    }
    EXPECT_TRUE(copy_reader.ParseNextBatch().empty());

    std::filesystem::remove(path_db, ec);
    std::filesystem::remove(path_out_csv, ec);
}

TEST(ReadWrite, RoundTripBatchAndEOF) {
    const std::string path_db =
        ReadWriteTestOutputPath("kio_db_round_trip_test.kiodb").string();

    std::error_code ec;
    std::filesystem::remove(path_db, ec);

    Schema schema({{"str", "string"}, {"integers", "int64"}});
    ctp::ColumnarBatch expected = {
        ctp::Column{std::vector<std::string>{"alpha", "", "hello, csv"}},
        ctp::Column{std::vector<int64_t>{42, -7, 0}}};

    {
        KioDbWriter writer(path_db, schema);
        ASSERT_NO_THROW(writer.WriteBatchToFile(expected));
    }

    KioDbReader reader(path_db);
    std::optional<KioReadBatch> actual = reader.ReadNextBatch();
    ASSERT_TRUE(actual.has_value());
    EXPECT_EQ(actual->columns, expected);
    EXPECT_EQ(actual->row_count, 3u);
    EXPECT_FALSE(reader.ReadNextBatch().has_value());

    std::filesystem::remove(path_db, ec);
}

TEST(ReadWrite, RoundTripsDoubleColumns) {
    const std::string path_db =
        ReadWriteTestOutputPath("kio_db_double_round_trip_test.kiodb").string();

    std::error_code ec;
    std::filesystem::remove(path_db, ec);

    Schema schema({{"score", "DOUBLE"}, {"id", "int64"}});
    ctp::ColumnarBatch expected = {
        ctp::Column{std::vector<double>{1.5, -2.25, 0.0}},
        ctp::Column{std::vector<int64_t>{1, 2, 3}}};

    {
        KioDbWriter writer(path_db, schema);
        ASSERT_NO_THROW(writer.WriteBatchToFile(expected));
    }

    KioDbReader reader(path_db);
    std::optional<KioReadBatch> actual = reader.ReadNextBatch();
    ASSERT_TRUE(actual.has_value());
    EXPECT_EQ(actual->columns, expected);
    EXPECT_EQ(actual->row_count, 3u);

    std::filesystem::remove(path_db, ec);
}

TEST(ReadWrite, ReadsProjectedColumns) {
    const std::string path_db =
        ReadWriteTestOutputPath("kio_db_projected_read_test.kiodb").string();

    std::error_code ec;
    std::filesystem::remove(path_db, ec);

    Schema schema({{"str", "string"}, {"integers", "int64"}, {"small", "SMALLINT"}});
    ctp::ColumnarBatch first_batch = {
        ctp::Column{std::vector<std::string>{"alpha", "beta"}},
        ctp::Column{std::vector<int64_t>{10, 20}},
        ctp::Column{std::vector<int16_t>{1, 2}}};
    ctp::ColumnarBatch second_batch = {
        ctp::Column{std::vector<std::string>{"gamma", "delta"}},
        ctp::Column{std::vector<int64_t>{30, 40}},
        ctp::Column{std::vector<int16_t>{3, 4}}};

    {
        KioDbWriter writer(path_db, schema);
        writer.WriteBatchToFile(first_batch);
        writer.WriteBatchToFile(second_batch);
    }

    KioDbReader reader(path_db);
    ctp::ColumnarBatch expected_first = {
        ctp::Column{std::vector<int16_t>{1, 2}},
        ctp::Column{std::vector<std::string>{"alpha", "beta"}}};
    std::optional<KioReadBatch> actual_first = reader.ReadNextBatch({2, 0});
    ASSERT_TRUE(actual_first.has_value());
    EXPECT_EQ(actual_first->columns, expected_first);
    EXPECT_EQ(actual_first->row_count, 2u);

    ctp::ColumnarBatch expected_second = {
        ctp::Column{std::vector<int64_t>{30, 40}}};
    std::optional<KioReadBatch> actual_second = reader.ReadNextBatch({1});
    ASSERT_TRUE(actual_second.has_value());
    EXPECT_EQ(actual_second->columns, expected_second);
    EXPECT_EQ(actual_second->row_count, 2u);
    EXPECT_FALSE(reader.ReadNextBatch({1}).has_value());

    std::filesystem::remove(path_db, ec);
}

TEST(ReadWrite, StoresSelfContainedFooterMetadata) {
    const std::string path_db =
        ReadWriteTestOutputPath("kio_db_footer_metadata_test.kiodb").string();

    std::error_code ec;
    std::filesystem::remove(path_db, ec);

    Schema schema({{"str", "string"}, {"integers", "int64"}});
    ctp::ColumnarBatch batch = {
        ctp::Column{std::vector<std::string>{"beta", "alpha"}},
        ctp::Column{std::vector<int64_t>{10, 20}}};

    {
        KioDbWriter writer(path_db, schema);
        writer.WriteBatchToFile(batch);
        writer.Finalize();
    }

    KioDbReader reader(path_db);
    const kio::FileMetadata& metadata = reader.GetMetadata();
    EXPECT_EQ(metadata.row_count, 2u);
    EXPECT_EQ(metadata.schema.ColumnCount(), 2u);
    EXPECT_EQ(metadata.schema.ColumnName(0), "str");
    EXPECT_EQ(metadata.schema.ColumnType(1), Schema::BIGINT);
    ASSERT_EQ(metadata.row_groups.size(), 1u);
    EXPECT_EQ(metadata.row_groups[0].batch.row_num, 2u);
    EXPECT_EQ(metadata.row_groups[0].batch.batch_start_offset,
              sizeof(kio::kMagic) + sizeof(uint64_t));
    ASSERT_EQ(metadata.row_groups[0].columns.size(), 2u);
    EXPECT_EQ(metadata.row_groups[0].columns[0].local_offset, 0u);
    EXPECT_EQ(metadata.row_groups[0].columns[1].local_offset,
              metadata.row_groups[0].columns[0].size);
    EXPECT_EQ(metadata.row_groups[0].columns[0].encoding,
              kio::Encoding::DICTIONARY);
    EXPECT_EQ(metadata.row_groups[0].columns[0].compression, kio::Compression::NONE);
    EXPECT_EQ(metadata.row_groups[0].columns[1].encoding,
              kio::Encoding::DELTA);
    EXPECT_TRUE(metadata.row_groups[0].columns[1].has_min_max);
    EXPECT_EQ(metadata.row_groups[0].columns[1].min_value, "10");
    EXPECT_EQ(metadata.row_groups[0].columns[1].max_value, "20");

    std::filesystem::remove(path_db, ec);
}

TEST(ReadWrite, RejectsInvalidMagic) {
    const std::string path_db =
        ReadWriteTestOutputPath("kio_db_invalid_magic_test.kiodb").string();

    std::error_code ec;
    std::filesystem::remove(path_db, ec);

    {
        std::ofstream output(path_db, std::ios::binary);
        ASSERT_TRUE(output.is_open());
        output.write("BAD!", 4);
        const uint64_t footer_offset = 12;
        output.write(reinterpret_cast<const char*>(&footer_offset),
                     sizeof(footer_offset));
    }

    EXPECT_THROW(KioDbReader reader(path_db), std::runtime_error);

    std::filesystem::remove(path_db, ec);
}

TEST(ReadWrite, EmptyKioFileReturnsEmptyBatch) {
    const std::string path_db =
        ReadWriteTestOutputPath("kio_db_empty_file_test.kiodb").string();

    std::error_code ec;
    std::filesystem::remove(path_db, ec);

    Schema schema(std::vector<std::vector<std::string> >{{"str", "string"}});
    {
        KioDbWriter writer(path_db, schema);
        writer.Finalize();
    }

    KioDbReader reader(path_db);
    EXPECT_FALSE(reader.ReadNextBatch().has_value());

    std::filesystem::remove(path_db, ec);
}
