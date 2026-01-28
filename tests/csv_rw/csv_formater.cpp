#include <csv_work/csv_formatter.h>
#include <kio_work/kio_db_reader.h>
#include "gtest/gtest.h"

#include <fstream>
#include <filesystem>

TEST(BatchFormatterTest, MakeFileColumnar) {
    const std::string path_csv =
        "/home/ivan/CLionProjects/KIO_DB/tests/csv_rw/test_csv1.csv";
    const std::string path_schema =
        "/home/ivan/CLionProjects/KIO_DB/tests/csv_rw/test_schema1.csv";

    CSVFormatter reader(path_csv, path_schema);
    ctp::ColumnarBatch expected = {
        ctp::Column{std::vector<std::string>{"a", "b"}},
        ctp::Column{std::vector<int64_t>{1, 2}}};
    EXPECT_EQ(reader.MakeColumnarBatch(), expected);

    expected.clear();
    EXPECT_EQ(reader.MakeColumnarBatch(), expected);

    Schema schema(reader.GetSchema());
    std::unordered_map<std::string, size_t> expected_names_to_index = {
        {"str", 0}, {"integers", 1}};
    std::vector<Schema::Types> expected_index_to_types{Schema::STRING,
                                                       Schema::INT64};
    std::vector<std::string> expected_index_to_names{"str", "integers"};

    EXPECT_EQ(reader.GetSchema().GetIndexToName(), expected_index_to_names);
    EXPECT_EQ(reader.GetSchema().GetIndexToType(), expected_index_to_types);
    EXPECT_EQ(reader.GetSchema().GetNameToIndex(), expected_names_to_index);
}
