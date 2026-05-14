#include <filesystem>
#include <fstream>
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
        Schema schema = LoadReadWriteTestSchema(path_schema);
        KioDbReader reader(path_db, schema);
        ASSERT_NO_THROW({
            CsvExporter exporter(reader, path_out_csv);
            exporter.Export();
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

    KioDbReader reader(path_db, schema);
    EXPECT_EQ(reader.ReadNextBatch(), expected);
    EXPECT_TRUE(reader.ReadNextBatch().empty());

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

    KioDbReader reader(path_db, schema);
    ctp::ColumnarBatch expected_first = {
        ctp::Column{std::vector<int16_t>{1, 2}},
        ctp::Column{std::vector<std::string>{"alpha", "beta"}}};
    EXPECT_EQ(reader.ReadNextProjectedBatch({2, 0}), expected_first);

    ctp::ColumnarBatch expected_second = {
        ctp::Column{std::vector<int64_t>{30, 40}}};
    EXPECT_EQ(reader.ReadNextProjectedBatch({1}), expected_second);
    EXPECT_TRUE(reader.ReadNextProjectedBatch({1}).empty());

    std::filesystem::remove(path_db, ec);
}

TEST(ReadWrite, EmptyFileReturnsEmptyBatch) {
    const std::string path_db =
        ReadWriteTestOutputPath("kio_db_empty_file_test.kiodb").string();

    std::error_code ec;
    std::filesystem::remove(path_db, ec);

    {
        std::ofstream out(path_db, std::ios::binary);
        ASSERT_TRUE(out.is_open());
    }

    Schema schema(std::vector<std::vector<std::string> >{{"str", "string"}});
    KioDbReader reader(path_db, schema);
    EXPECT_TRUE(reader.ReadNextBatch().empty());

    std::filesystem::remove(path_db, ec);
}
