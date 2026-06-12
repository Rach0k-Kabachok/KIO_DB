#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

#include "gtest/gtest.h"

#include "columnar_types.h"
#include "schema.h"
#include "transport/csv/csv_batch_reader.h"
#include "transport/csv/csv_exporter.h"
#include "transport/kio/kio_db_importer.h"
#include "transport/kio/kio_db_reader.h"
#include "transport/kio/kio_db_writer.h"

namespace {

std::filesystem::path RepoRoot() {
    return std::filesystem::path(__FILE__).parent_path().parent_path()
        .parent_path();
}

std::filesystem::path HitsCsvPath() {
    return RepoRoot() / "Testing" / "hits.csv";
}

std::filesystem::path HitsSchemaPath() {
    return RepoRoot() / "tests" / "hits_schema.csv";
}

std::filesystem::path TestOutputPath(const std::string& filename) {
    const std::filesystem::path output_dir =
        RepoRoot() / "Testing" / "round_trip_output";
    std::filesystem::create_directories(output_dir);
    return output_dir / filename;
}

bool EnvEnabled(std::string_view name) {
    const char* value = std::getenv(std::string(name).c_str());
    return value != nullptr && std::string_view(value) == "1";
}

class ParsedCsvRowStream {
public:
    explicit ParsedCsvRowStream(const std::filesystem::path& csv_path)
        : reader_(csv_path.string()) {
    }

    bool Next(ctp::ParsedRow& row) {
        while (row_index_ >= batch_.size()) {
            batch_ = reader_.ParseNextBatch();
            row_index_ = 0;

            if (batch_.empty()) {
                return false;
            }
        }

        row = batch_[row_index_++];
        return true;
    }

private:
    CSVBatchReader reader_;
    ctp::ParsedBatch batch_;
    size_t row_index_ = 0;
};

void CopyFirstCsvRows(const std::filesystem::path& input_path,
                      const std::filesystem::path& output_path,
                      size_t row_count) {
    std::ifstream input(input_path, std::ios::binary);
    ASSERT_TRUE(input.is_open()) << "Cannot open " << input_path;

    std::ofstream output(output_path, std::ios::binary);
    ASSERT_TRUE(output.is_open()) << "Cannot open " << output_path;

    bool in_quote = false;
    size_t copied_rows = 0;
    char ch = '\0';

    while (copied_rows < row_count && input.get(ch)) {
        output.put(ch);

        if (ch == '"') {
            if (in_quote && input.peek() == '"') {
                input.get(ch);
                output.put(ch);
            } else {
                in_quote = !in_quote;
            }
        } else if (ch == '\n' && !in_quote) {
            ++copied_rows;
        }
    }

    ASSERT_EQ(copied_rows, row_count);
}

::testing::AssertionResult CsvFilesHaveSameParsedRows(
    const std::filesystem::path& lhs_path,
    const std::filesystem::path& rhs_path) {
    ParsedCsvRowStream lhs(lhs_path);
    ParsedCsvRowStream rhs(rhs_path);

    ctp::ParsedRow lhs_row;
    ctp::ParsedRow rhs_row;
    size_t row_index = 0;

    while (true) {
        const bool lhs_has_row = lhs.Next(lhs_row);
        const bool rhs_has_row = rhs.Next(rhs_row);

        if (lhs_has_row != rhs_has_row) {
            return ::testing::AssertionFailure()
                   << "CSV row count differs at row " << row_index;
        }

        if (!lhs_has_row) {
            return ::testing::AssertionSuccess();
        }

        if (lhs_row != rhs_row) {
            return ::testing::AssertionFailure()
                   << "CSV rows differ at row " << row_index << ": left has "
                   << lhs_row.size() << " fields, right has " << rhs_row.size()
                   << " fields";
        }

        ++row_index;
    }
}

void RoundTripCsvToKioAndBack(const std::filesystem::path& input_csv,
                              const std::filesystem::path& schema_csv,
                              const std::filesystem::path& kio_db,
                              const std::filesystem::path& output_csv) {
    Schema schema(schema_csv.string());

    {
        KioDbWriter writer(kio_db.string(), schema);
        KioDbImporter importer(input_csv.string(), schema, writer);
        importer.Import();
    }

    {
        KioDbReader reader(kio_db.string());
        CsvExporter exporter(output_csv.string());
        exporter.ExportFile(reader);
    }
}

bool HitsFilesArePresent(const std::filesystem::path& hits_csv,
                         const std::filesystem::path& hits_schema) {
    if (!std::filesystem::exists(hits_csv)) {
        return false;
    }
    if (!std::filesystem::exists(hits_schema)) {
        return false;
    }
    return true;
}

}  // namespace

TEST(ReadWrite, HitsCsvSampleRoundTrip) {
    const std::filesystem::path hits_csv = HitsCsvPath();
    const std::filesystem::path hits_schema = HitsSchemaPath();
    if (!HitsFilesArePresent(hits_csv, hits_schema)) {
        GTEST_SKIP() << "Missing optional hits.csv sample or schema";
    }

    const std::filesystem::path sample_csv =
        TestOutputPath("kio_db_hits_round_trip_sample.csv");
    const std::filesystem::path kio_db =
        TestOutputPath("kio_db_hits_round_trip_sample.kiodb");
    const std::filesystem::path output_csv =
        TestOutputPath("kio_db_hits_round_trip_sample_out.csv");

    std::error_code ec;
    std::filesystem::remove(sample_csv, ec);
    std::filesystem::remove(kio_db, ec);
    std::filesystem::remove(output_csv, ec);

    CopyFirstCsvRows(hits_csv, sample_csv, 32);
    ASSERT_NO_THROW(
        RoundTripCsvToKioAndBack(sample_csv, hits_schema, kio_db, output_csv));

    ASSERT_TRUE(std::filesystem::exists(output_csv));
    EXPECT_TRUE(CsvFilesHaveSameParsedRows(sample_csv, output_csv))
        << "hits sample changed after csv -> kiodb -> csv";

    std::filesystem::remove(sample_csv, ec);
    std::filesystem::remove(kio_db, ec);
    std::filesystem::remove(output_csv, ec);
}

TEST(ReadWrite, HitsCsvFullRoundTrip) {
    const std::filesystem::path hits_csv = HitsCsvPath();
    const std::filesystem::path hits_schema = HitsSchemaPath();
    if (!HitsFilesArePresent(hits_csv, hits_schema)) {
        GTEST_SKIP() << "Missing optional hits.csv sample or schema";
    }

    if (!EnvEnabled("KIO_DB_RUN_HITS_ROUND_TRIP")) {
        GTEST_SKIP()
            << "Set KIO_DB_RUN_HITS_ROUND_TRIP=1 to run the full hits.csv "
               "round-trip";
    }

    const std::filesystem::path kio_db =
        TestOutputPath("kio_db_hits_full_round_trip.kiodb");
    const std::filesystem::path output_csv =
        TestOutputPath("kio_db_hits_full_round_trip.csv");

    std::error_code ec;
    std::filesystem::remove(kio_db, ec);
    std::filesystem::remove(output_csv, ec);

    ASSERT_NO_THROW(
        RoundTripCsvToKioAndBack(hits_csv, hits_schema, kio_db, output_csv));

    ASSERT_TRUE(std::filesystem::exists(output_csv));
    EXPECT_TRUE(CsvFilesHaveSameParsedRows(hits_csv, output_csv))
        << "hits.csv changed after csv -> kiodb -> csv";

    std::filesystem::remove(kio_db, ec);
    std::filesystem::remove(output_csv, ec);
}
