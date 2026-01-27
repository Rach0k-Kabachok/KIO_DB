#include <csv_foramtter.h>
#include <kio_db_reader.h>
#include "gtest/gtest.h"

#include <fstream>
#include <filesystem>

TEST(BatchFormatterTest, MakeFileColumnar) {
    const std::string path_csv = "batch_reader_test.csv";
    const std::string path_schema = "schema_test.csv";


    {
        std::ofstream out_csv(path_csv);
        ASSERT_TRUE(out_csv.is_open());
        out_csv << "a,1\nb,2\n";
    }

    {
        std::ofstream out_schema(path_schema);
        ASSERT_TRUE(out_schema.is_open());
        out_schema << "str,string\nintegers,int64";
    }

    CSVFormatter reader(path_csv, path_schema);
    Kio::ColumnarBatch expected = {
        Kio::Column{std::vector<std::string>{"a", "b"}},
        Kio::Column{std::vector<int64_t>{1, 2}}
    };
    EXPECT_EQ(reader.MakeColumnarBatch(), expected);

    expected.clear();
    EXPECT_EQ(reader.MakeColumnarBatch(), expected);

    Schema schema(reader.GetSchema());
    std::unordered_map<std::string, size_t> expected_names_to_index = {
        {"str", 0},
        {"integers", 1}
    };
    std::vector<Schema::Types> expected_index_to_types{Schema::STRING, Schema::INT64};
    std::vector<std::string> expected_index_to_names{"str", "integers"};

    EXPECT_EQ(reader.GetSchema().GetIndexToName(), expected_index_to_names);
    EXPECT_EQ(reader.GetSchema().GetIndexToType(), expected_index_to_types);
    EXPECT_EQ(reader.GetSchema().GetNameToIndex(), expected_names_to_index);


    ASSERT_TRUE(std::filesystem::remove(path_schema));
    ASSERT_TRUE(std::filesystem::remove(path_csv));

}
