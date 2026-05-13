#include <csv_work/csv_formatter.h>
#include <csv_work/csv_batch_reader.h>
#include <kio_work/kio_db_reader.h>
#include "gtest/gtest.h"

#include <filesystem>
#include <fstream>

std::filesystem::path FormatterTestDataDir() {
    return std::filesystem::path(__FILE__).parent_path();
}

Schema LoadFormatterTestSchema(const std::string& path_schema) {
    CSVBatchReader schema_reader(path_schema);
    ctp::ParsedBatch schema_rows;
    ctp::ParsedBatch batch;

    while (!(batch = schema_reader.ParseNextBatch()).empty()) {
        schema_rows.insert(schema_rows.end(), batch.begin(), batch.end());
    }

    return Schema(schema_rows);
}

TEST(BatchFormatterTest, MakeFileColumnar) {
    const std::string path_csv =
        (FormatterTestDataDir() / "test_csv1.csv").string();
    const std::string path_schema =
        (FormatterTestDataDir() / "test_schema1.csv").string();

    Schema schema = LoadFormatterTestSchema(path_schema);
    CSVFormatter reader(path_csv, schema);
    ctp::ColumnarBatch expected = {
        ctp::Column{std::vector<std::string>{"a", "b"}},
        ctp::Column{std::vector<int64_t>{1, 2}}};
    EXPECT_EQ(reader.MakeColumnarBatch(), expected);

    expected.clear();
    EXPECT_EQ(reader.MakeColumnarBatch(), expected);

    std::unordered_map<std::string, size_t> expected_names_to_index = {
        {"str", 0}, {"integers", 1}};
    std::vector<Schema::Types> expected_index_to_types{Schema::STRING,
                                                       Schema::INT64};
    std::vector<std::string> expected_index_to_names{"str", "integers"};

    EXPECT_EQ(schema.GetIndexToName(), expected_index_to_names);
    EXPECT_EQ(schema.GetIndexToType(), expected_index_to_types);
    EXPECT_EQ(schema.GetNameToIndex(), expected_names_to_index);
}
