#include "csv_batch_reader.h"
#include "gtest/gtest.h"

#include <fstream>
#include <filesystem>

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
