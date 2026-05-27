#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "gtest/gtest.h"

#include "execution/operators.h"
#include "operator_test_utils.h"

namespace {

using test_exec::MakeBatch;
using test_exec::MakeInput;
using test_exec::MakeInt64ValueInput;
using test_exec::MakeSchema;
using test_exec::ReadAllValues;
using test_exec::Values;

ExecBatch MakeNameValueBatch(std::shared_ptr<const Schema> schema,
                             std::vector<std::string> names,
                             std::vector<int64_t> values) {
    return MakeBatch(
        std::move(schema),
        ctp::ColumnarBatch{ctp::Column{std::move(names)},
                           ctp::Column{std::move(values)}});
}

}  // namespace

TEST(LimitOperatorTest, JustWorksWithOffsetAcrossBatches) {
    LimitOperator op(MakeInt64ValueInput({{1, 2, 3}, {4, 5, 6}, {7, 8}}),
                     3, 2);
    EXPECT_EQ(ReadAllValues<int64_t>(op), std::vector<int64_t>({3, 4, 5}));
}

TEST(LimitOperatorTest, EmptyWhenOffsetIsPastInput) {
    LimitOperator op(MakeInt64ValueInput({{1, 2, 3}, {4, 5}}), 3, 100);
    EXPECT_TRUE(ReadAllValues<int64_t>(op).empty());
}

TEST(FilterOperatorTest, JustWorksAndKeepsInputSchema) {
    auto schema = MakeSchema({{"name", "string"}, {"value", "int64"}});

    FilterOperator op(
        MakeInput({MakeNameValueBatch(schema, {"a", "b", "c"},
                                      {10, 20, 30})}),
        [](const ExecBatch& batch, size_t row_idx) {
            return Values<int64_t>(batch, 1)[row_idx] >= 20;
        });

    std::optional<ExecBatch> batch = op.Next();
    ASSERT_TRUE(batch.has_value());
    EXPECT_EQ(batch->schema, schema);
    EXPECT_EQ(Values<std::string>(*batch, 0),
              std::vector<std::string>({"b", "c"}));
    EXPECT_EQ(Values<int64_t>(*batch, 1), std::vector<int64_t>({20, 30}));
}

TEST(FilterOperatorTest, ReturnsEmptyColumnsWhenNothingMatches) {
    auto schema = MakeSchema({{"name", "string"}, {"value", "int64"}});

    FilterOperator op(
        MakeInput({MakeNameValueBatch(schema, {"a", "b", "c"},
                                      {10, 20, 30})}),
        [](const ExecBatch&, size_t) { return false; });

    std::optional<ExecBatch> batch = op.Next();
    ASSERT_TRUE(batch.has_value());
    EXPECT_EQ(batch->schema, schema);
    EXPECT_EQ(batch->row_count, 0u);
    EXPECT_TRUE(Values<std::string>(*batch, 0).empty());
    EXPECT_TRUE(Values<int64_t>(*batch, 1).empty());
}

TEST(ProjectOperatorTest, JustWorksAndReusesProjectedSchema) {
    auto input_schema = MakeSchema({{"name", "string"}, {"value", "int64"}});

    ProjectOperator op(
        MakeInput({MakeNameValueBatch(input_schema, {"a", "b"}, {10, 20}),
                   MakeNameValueBatch(input_schema, {"c"}, {30})}),
        {"value"});

    std::optional<ExecBatch> first = op.Next();
    std::optional<ExecBatch> second = op.Next();

    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(first->schema, second->schema);
    EXPECT_EQ(first->schema->ColumnCount(), 1u);
    EXPECT_EQ(first->schema->ColumnName(0), "value");
    EXPECT_EQ(first->schema->ColumnType(0), Schema::BIGINT);
    EXPECT_EQ(Values<int64_t>(*first, 0), std::vector<int64_t>({10, 20}));
    EXPECT_EQ(Values<int64_t>(*second, 0), std::vector<int64_t>({30}));
}

TEST(ComputeOperatorTest, JustWorks) {
    auto schema = MakeSchema({{"value", "int64"}});
    ExecBatch input = MakeBatch(
        schema,
        ctp::ColumnarBatch{ctp::Column{std::vector<int64_t>{1, 2, 3}}});

    ComputeOperator op(
        MakeInput({std::move(input)}),
        {{"twice", Schema::BIGINT,
          [](const ExecBatch& batch, size_t row_idx) -> scalar::Value {
              return Values<int64_t>(batch, 0)[row_idx] * 2;
          }}});

    std::optional<ExecBatch> result = op.Next();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->schema->ColumnName(1), "twice");
    EXPECT_EQ(Values<int64_t>(*result, 1), std::vector<int64_t>({2, 4, 6}));
    EXPECT_FALSE(op.Next().has_value());
}
