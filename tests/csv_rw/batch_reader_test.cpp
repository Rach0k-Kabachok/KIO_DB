#include "csv_work/csv_batch_reader.h"
#include "gtest/gtest.h"

#include <fstream>
#include <filesystem>
#include <string>
#include <vector>

TEST(BatchReaderTest, BatchFiles) {
    const std::string path = "batch_reader_test.csv";

    {
        std::ofstream out(path);
        ASSERT_TRUE(out.is_open());
        out << "a,b\n1,2\n";
    }

    CSVBatchReader reader(path);
    std::vector<std::vector<std::string>> expected = {{"a", "b"}, {"1", "2"}};
    EXPECT_EQ(reader.ParseNextBatch(), expected);

    expected.clear();
    EXPECT_EQ(reader.ParseNextBatch(), expected);

    ASSERT_TRUE(std::filesystem::remove(path));
}

TEST(BatchReaderTest, NewLineInQuotedFieldAcrossBatchBoundary) {
    const std::string path = "batch_reader_multiline_field_test.csv";
    const std::string long_prefix((1 << 20) + 32, 'x');

    {
        std::ofstream out(path, std::ios::binary);
        ASSERT_TRUE(out.is_open());
        out << "id,text\n";
        out << "1,\"" << long_prefix << "\ninside field\"\n";
        out << "2,plain\n";
    }

    CSVBatchReader reader(path);
    std::vector<std::vector<std::string>> parsed;
    std::vector<std::vector<std::string>> batch;
    while (!(batch = reader.ParseNextBatch()).empty()) {
        parsed.insert(parsed.end(), batch.begin(), batch.end());
    }

    std::vector<std::vector<std::string>> expected = {
        {"id", "text"}, {"1", long_prefix + "\ninside field"}, {"2", "plain"}};
    EXPECT_EQ(parsed, expected);

    ASSERT_TRUE(std::filesystem::remove(path));
}
