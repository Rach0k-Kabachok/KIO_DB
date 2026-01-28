#include <filesystem>
#include <string>

#include "gtest/gtest.h"

#include "kio_work/kio_db.h"
#include "kio_work/kio_db_reader.h"
#include "csv_work/csv_exporter.h"

#include "csv_work/csv_batch_reader.h"

TEST(ReadWrite, JustWorks) {
    const std::string path_csv =
        "/home/ivan/CLionProjects/KIO_DB/tests/csv_rw/test_csv1.csv";
    const std::string path_schema =
        "/home/ivan/CLionProjects/KIO_DB/tests/csv_rw/test_schema1.csv";

    const std::string path_db =
        "/home/ivan/CLionProjects/KIO_DB/tests/csv_rw/test_db1.kiodb";

    const std::string path_out_csv =
        "/home/ivan/CLionProjects/KIO_DB/tests/csv_rw/test_output_csv1.csv";

    std::error_code ec;
    std::filesystem::remove(path_db, ec);
    std::filesystem::remove(path_out_csv, ec);

    {
        KioDb db(path_csv, path_schema, path_db);
        ASSERT_NO_THROW(db.ProcessAllInput());
    }

    ASSERT_TRUE(std::filesystem::exists(path_db));
    ASSERT_GT(std::filesystem::file_size(path_db), 0u);

    {
        KioDbReader reader(path_db);
        ASSERT_NO_THROW(CsvExporter::Export(reader, path_out_csv));
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
}
