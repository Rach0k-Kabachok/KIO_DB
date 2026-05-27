#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "gtest/gtest.h"

#include "execution/clickbench_queries.h"
#include "execution/operators.h"
#include "execution/result_writer.h"
#include "global/column_operations.h"
#include "global/columnar_types.h"
#include "global/schema.h"
#include "operator_test_utils.h"
#include "transport/kio/kio_db_importer.h"
#include "transport/kio/kio_db_writer.h"

namespace {

using test_exec::MakeBatch;
using test_exec::MakeInput;
using test_exec::Values;

std::filesystem::path RepoRoot() {
    return std::filesystem::path(__FILE__).parent_path().parent_path()
        .parent_path();
}

std::filesystem::path TestOutputPath(const std::string& filename) {
    return std::filesystem::temp_directory_path() / filename;
}

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
            copied_rows++;
        }
    }

    ASSERT_EQ(copied_rows, row_count);
}

void ExpectConsistentBatch(const ExecBatch& batch) {
    ASSERT_TRUE(batch.schema);
    ASSERT_EQ(batch.columns.size(), batch.schema->ColumnCount());
    for (const ctp::Column& column : batch.columns) {
        EXPECT_EQ(ctp::GetColumnRowCount(column), batch.row_count);
    }
}

}  // namespace

TEST(ResultWriterTest, WritesAllChildBatchesAsCsv) {
    const std::filesystem::path output_path =
        TestOutputPath("kio_db_result_writer_operator.csv");
    std::error_code ec;
    std::filesystem::remove(output_path, ec);

    auto schema = std::make_shared<Schema>(
        Schema::FromColumns({"id", "name"},
                            {Schema::BIGINT, Schema::TEXT}));
    ExecBatch first = MakeBatch(
        schema,
        ctp::ColumnarBatch{
            ctp::Column{std::vector<int64_t>{1, 2}},
            ctp::Column{std::vector<std::string>{"alpha", "b,b"}}});
    ExecBatch second = MakeBatch(
        schema,
        ctp::ColumnarBatch{
            ctp::Column{std::vector<int64_t>{3}},
            ctp::Column{std::vector<std::string>{"quote\"me"}}});

    {
        ResultWriterOperator op(
            MakeInput({std::move(first), std::move(second)}),
            output_path.string());

        EXPECT_FALSE(op.Next().has_value());
        EXPECT_FALSE(op.Next().has_value());
    }

    std::ifstream input(output_path);
    ASSERT_TRUE(input.is_open());
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(input, line)) {
        lines.push_back(line);
    }

    EXPECT_EQ(lines, std::vector<std::string>({
                         "1,\"alpha\"",
                         "2,\"b,b\"",
                         "3,\"quote\"\"me\""}));

    std::filesystem::remove(output_path, ec);
}

TEST(TableScanOperatorTest, UsesMinMaxConstraintsToSkipRowGroups) {
    const std::filesystem::path db_path =
        TestOutputPath("kio_db_min_max_skip_test.kiodb");
    std::error_code ec;
    std::filesystem::remove(db_path, ec);

    Schema schema(std::vector<std::vector<std::string>>{{"value", "int64"}});
    {
        KioDbWriter writer(db_path.string(), schema);
        writer.WriteBatchToFile(
            ctp::ColumnarBatch{ctp::Column{std::vector<int64_t>{1, 2}}});
        writer.WriteBatchToFile(
            ctp::ColumnarBatch{ctp::Column{std::vector<int64_t>{10, 11}}});
    }

    auto constraints = std::make_shared<std::vector<MinMaxConstraint>>();
    constraints->push_back(MinMaxConstraint{
        "value", Schema::BIGINT, int64_t{10}, int64_t{10},
        true, true, false, int64_t{0}});

    TableScanOperator scan(db_path.string(), {"value"}, constraints);
    std::optional<ExecBatch> result = scan.Next();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(Values<int64_t>(*result, 0), std::vector<int64_t>({10, 11}));
    EXPECT_FALSE(scan.Next().has_value());

    std::filesystem::remove(db_path, ec);
}

TEST(ClickBenchQueryTest, ExecutesAllQueryIdsOnHitsSample) {
    const std::filesystem::path hits_csv = RepoRoot() / "Testing" / "hits.csv";
    const std::filesystem::path hits_schema =
        RepoRoot() / "tests" / "hits_schema.csv";
    if (!std::filesystem::exists(hits_csv)) {
        GTEST_SKIP() << "Missing optional ClickBench sample: " << hits_csv;
    }
    if (!std::filesystem::exists(hits_schema)) {
        GTEST_SKIP() << "Missing tracked ClickBench schema: " << hits_schema;
    }

    const std::filesystem::path sample_csv =
        TestOutputPath("kio_db_clickbench_sample.csv");
    const std::filesystem::path db_path =
        TestOutputPath("kio_db_clickbench_sample.kiodb");
    std::error_code ec;
    std::filesystem::remove(sample_csv, ec);
    std::filesystem::remove(db_path, ec);

    CopyFirstCsvRows(hits_csv, sample_csv, 8);

    {
        Schema schema(hits_schema.string());
        KioDbWriter writer(db_path.string(), schema);
        KioDbImporter importer(sample_csv.string(), schema, writer);
        ASSERT_NO_THROW(importer.Import());
    }

    for (int query_id = 1; query_id <= 43; query_id++) {
        SCOPED_TRACE(query_id);
        ExecBatch result = ExecuteClickBenchQuery(db_path.string(), query_id);
        ExpectConsistentBatch(result);
    }

    std::filesystem::remove(sample_csv, ec);
    std::filesystem::remove(db_path, ec);
}
