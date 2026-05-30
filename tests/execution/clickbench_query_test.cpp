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

void WriteSingleBatchDb(const std::filesystem::path& db_path,
                        const Schema& schema,
                        const ctp::ColumnarBatch& batch) {
    std::error_code ec;
    std::filesystem::remove(db_path, ec);

    KioDbWriter writer(db_path.string(), schema);
    writer.WriteBatchToFile(batch);
    writer.Finalize();
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

TEST(ClickBenchQueryRewriteTest, UsesPartialSortingForQueries24To27) {
    const std::filesystem::path db_path =
        TestOutputPath("kio_db_clickbench_sort_rewrite.kiodb");
    std::error_code ec;

    const Schema schema = Schema::FromColumns(
        {"URL", "EventTime", "SearchPhrase"},
        {Schema::TEXT, Schema::TIMESTAMP, Schema::TEXT});
    WriteSingleBatchDb(
        db_path, schema,
        ctp::ColumnarBatch{
            ctp::Column{std::vector<std::string>{
                "http://google/a", "http://other", "https://google/b",
                "http://google/c", "http://none"}},
            ctp::Column{std::vector<int64_t>{30, 5, 20, 10, 15}},
            ctp::Column{std::vector<std::string>{
                "zeta", "skip", "beta", "alpha", "aardvark"}}});

    ExecBatch query24 = ExecuteClickBenchQuery(db_path.string(), 24);
    ExpectConsistentBatch(query24);
    EXPECT_EQ(query24.row_count, 3);
    EXPECT_EQ(Values<std::string>(query24, 0),
              std::vector<std::string>({"http://google/c",
                                        "https://google/b",
                                        "http://google/a"}));
    EXPECT_EQ(Values<int64_t>(query24, 1),
              std::vector<int64_t>({10, 20, 30}));

    ExecBatch query25 = ExecuteClickBenchQuery(db_path.string(), 25);
    ExpectConsistentBatch(query25);
    EXPECT_EQ(Values<std::string>(query25, 0),
              std::vector<std::string>({"skip", "alpha", "aardvark",
                                        "beta", "zeta"}));

    ExecBatch query26 = ExecuteClickBenchQuery(db_path.string(), 26);
    ExpectConsistentBatch(query26);
    EXPECT_EQ(Values<std::string>(query26, 0),
              std::vector<std::string>({"aardvark", "alpha", "beta",
                                        "skip", "zeta"}));

    ExecBatch query27 = ExecuteClickBenchQuery(db_path.string(), 27);
    ExpectConsistentBatch(query27);
    EXPECT_EQ(Values<std::string>(query27, 0),
              std::vector<std::string>({"skip", "alpha", "aardvark",
                                        "beta", "zeta"}));

    std::filesystem::remove(db_path, ec);
}

TEST(ClickBenchQueryRewriteTest, ComputesQuery30AfterAggregation) {
    const std::filesystem::path db_path =
        TestOutputPath("kio_db_clickbench_query30_rewrite.kiodb");
    std::error_code ec;

    const Schema schema = Schema::FromColumns(
        {"ResolutionWidth"}, {Schema::SMALLINT});
    WriteSingleBatchDb(
        db_path, schema,
        ctp::ColumnarBatch{
            ctp::Column{std::vector<int16_t>{100, 200, 300}}});

    ExecBatch result = ExecuteClickBenchQuery(db_path.string(), 30);
    ExpectConsistentBatch(result);
    ASSERT_EQ(result.row_count, 1);
    ASSERT_EQ(result.schema->ColumnCount(), 90);
    ASSERT_EQ(result.columns.size(), 90);

    for (size_t idx = 0; idx < 90; ++idx) {
        EXPECT_EQ(result.schema->ColumnName(idx),
                  "s" + std::to_string(idx));
        EXPECT_EQ(Values<int64_t>(result, idx),
                  std::vector<int64_t>({600 + static_cast<int64_t>(idx) * 3}));
    }

    std::filesystem::remove(db_path, ec);
}

TEST(ClickBenchQueryRewriteTest, PreservesQuery35OutputShape) {
    const std::filesystem::path db_path =
        TestOutputPath("kio_db_clickbench_query35_rewrite.kiodb");
    std::error_code ec;

    const Schema schema = Schema::FromColumns({"URL"}, {Schema::TEXT});
    WriteSingleBatchDb(
        db_path, schema,
        ctp::ColumnarBatch{
            ctp::Column{std::vector<std::string>{
                "a", "b", "a", "c", "b", "a"}}});

    ExecBatch result = ExecuteClickBenchQuery(db_path.string(), 35);
    ExpectConsistentBatch(result);
    EXPECT_EQ(result.row_count, 3);
    EXPECT_EQ(result.schema->ColumnName(0), "one");
    EXPECT_EQ(result.schema->ColumnName(1), "URL");
    EXPECT_EQ(result.schema->ColumnName(2), "c");
    EXPECT_EQ(Values<int64_t>(result, 0),
              std::vector<int64_t>({1, 1, 1}));
    EXPECT_EQ(Values<std::string>(result, 1),
              std::vector<std::string>({"a", "b", "c"}));
    EXPECT_EQ(Values<int64_t>(result, 2),
              std::vector<int64_t>({3, 2, 1}));

    std::filesystem::remove(db_path, ec);
}

TEST(ClickBenchQueryRewriteTest, PreservesQuery36OutputShape) {
    const std::filesystem::path db_path =
        TestOutputPath("kio_db_clickbench_query36_rewrite.kiodb");
    std::error_code ec;

    const Schema schema = Schema::FromColumns({"ClientIP"}, {Schema::INTEGER});
    WriteSingleBatchDb(
        db_path, schema,
        ctp::ColumnarBatch{
            ctp::Column{std::vector<int32_t>{10, 11, 10, 12, 11, 10}}});

    ExecBatch result = ExecuteClickBenchQuery(db_path.string(), 36);
    ExpectConsistentBatch(result);
    EXPECT_EQ(result.row_count, 3);
    EXPECT_EQ(result.schema->ColumnName(0), "ClientIP");
    EXPECT_EQ(result.schema->ColumnName(1), "ClientIPMinus1");
    EXPECT_EQ(result.schema->ColumnName(2), "ClientIPMinus2");
    EXPECT_EQ(result.schema->ColumnName(3), "ClientIPMinus3");
    EXPECT_EQ(result.schema->ColumnName(4), "c");
    EXPECT_EQ(Values<int32_t>(result, 0), std::vector<int32_t>({10, 11, 12}));
    EXPECT_EQ(Values<int32_t>(result, 1), std::vector<int32_t>({9, 10, 11}));
    EXPECT_EQ(Values<int32_t>(result, 2), std::vector<int32_t>({8, 9, 10}));
    EXPECT_EQ(Values<int32_t>(result, 3), std::vector<int32_t>({7, 8, 9}));
    EXPECT_EQ(Values<int64_t>(result, 4), std::vector<int64_t>({3, 2, 1}));

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
