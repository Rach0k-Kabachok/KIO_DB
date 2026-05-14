#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "gtest/gtest.h"

#include "schema.h"
#include "transport/csv/csv_batch_reader.h"
#include "transport/csv/csv_columnar_reader.h"

namespace {

std::filesystem::path ColumnarReaderTestDataDir() {
    return std::filesystem::path(__FILE__).parent_path();
}

std::filesystem::path ColumnarReaderTestOutputPath(
        const std::string& filename) {
    return std::filesystem::temp_directory_path() / filename;
}

Schema LoadColumnarReaderTestSchema(const std::string& path_schema) {
    CSVBatchReader schema_reader(path_schema);
    ctp::ParsedBatch schema_rows;
    ctp::ParsedBatch batch;

    while (!(batch = schema_reader.ParseNextBatch()).empty()) {
        schema_rows.insert(schema_rows.end(), batch.begin(), batch.end());
    }

    return Schema(schema_rows);
}

}  // namespace

TEST(CSVColumnarReaderTest, MakeFileColumnar) {
    const std::string path_csv =
        (ColumnarReaderTestDataDir() / "test_csv1.csv").string();
    const std::string path_schema =
        (ColumnarReaderTestDataDir() / "test_schema1.csv").string();

    Schema schema = LoadColumnarReaderTestSchema(path_schema);
    CSVColumnarReader reader(path_csv, schema);
    ctp::ColumnarBatch expected = {
        ctp::Column{std::vector<std::string>{"a", "b"}},
        ctp::Column{std::vector<int64_t>{1, 2}}};
    EXPECT_EQ(reader.MakeColumnarBatch(), expected);

    expected.clear();
    EXPECT_EQ(reader.MakeColumnarBatch(), expected);

    std::unordered_map<std::string, size_t> expected_names_to_index = {
        {"str", 0}, {"integers", 1}};
    std::vector<Schema::Types> expected_index_to_types{Schema::TEXT,
                                                       Schema::BIGINT};
    std::vector<std::string> expected_index_to_names{"str", "integers"};

    EXPECT_EQ(schema.GetIndexToName(), expected_index_to_names);
    EXPECT_EQ(schema.GetIndexToType(), expected_index_to_types);
    EXPECT_EQ(schema.GetNameToIndex(), expected_names_to_index);
}

TEST(CSVColumnarReaderTest, ParsesTypesAndQuotedStrings) {
    const std::filesystem::path path_csv =
        ColumnarReaderTestOutputPath("kio_db_columnar_reader_types.csv");

    {
        std::ofstream csv(path_csv, std::ios::binary);
        ASSERT_TRUE(csv.is_open());
        csv << "\"hello, csv\",42,7,\"1970-01-02\",\"1970-01-02 03:04:05\"\n";
        csv << "\"quote \"\"inside\"\"\",-5,1,\"1970-02-01\","
               "\"1970-02-01 00:00:01\"\n";
    }

    Schema schema({{"text_col", "string"},
                   {"big_col", "int64"},
                   {"small_col", "SMALLINT"},
                   {"date_col", "DATE"},
                   {"time_col", "TIMESTAMP"}});

    CSVColumnarReader reader(path_csv.string(), schema);
    ctp::ColumnarBatch expected = {
        ctp::Column{std::vector<std::string>{"hello, csv", "quote \"inside\""}},
        ctp::Column{std::vector<int64_t>{42, -5}},
        ctp::Column{std::vector<int16_t>{7, 1}},
        ctp::Column{std::vector<int32_t>{1, 31}},
        ctp::Column{std::vector<int64_t>{97445, 2678401}}};

    EXPECT_EQ(reader.MakeColumnarBatch(), expected);
    EXPECT_TRUE(reader.MakeColumnarBatch().empty());

    std::error_code ec;
    std::filesystem::remove(path_csv, ec);
}
