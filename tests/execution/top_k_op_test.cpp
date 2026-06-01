#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "gtest/gtest.h"

#include "execution/operators/sort_ops.h"
#include "operator_test_utils.h"

namespace {

using test_exec::MakeInput;
using test_exec::MakeInt64ValueInput;
using test_exec::MakeSchema;
using test_exec::MakeScoreNameBatch;
using test_exec::Values;

}  // namespace

TEST(TopKOperatorTest, JustWorksAcrossBatches) {
    TopKOperator op(MakeInt64ValueInput({{5, 1, 8}, {3, 2}, {7}}),
                    {SortKey{"value", SortOrder::ASC}}, 3);

    std::optional<ExecBatch> batch = op.Next();
    ASSERT_TRUE(batch.has_value());
    EXPECT_EQ(Values<int64_t>(*batch, 0), std::vector<int64_t>({1, 2, 3}));
    EXPECT_EQ(batch->row_count, 3u);
    EXPECT_FALSE(op.Next().has_value());
}

TEST(TopKOperatorTest, SupportsDescAndTieBreakKeys) {
    auto schema = MakeSchema({{"score", "int64"}, {"name", "string"}});

    std::vector<ExecBatch> batches;
    batches.push_back(MakeScoreNameBatch(schema, {9, 10}, {"b", "c"}));
    batches.push_back(MakeScoreNameBatch(schema, {10, 8, 10}, {"a", "z", "d"}));

    TopKOperator op(
        MakeInput(std::move(batches)),
        {SortKey{"score", SortOrder::DESC}, SortKey{"name", SortOrder::ASC}},
        3);

    std::optional<ExecBatch> batch = op.Next();
    ASSERT_TRUE(batch.has_value());
    EXPECT_EQ(Values<int64_t>(*batch, 0), std::vector<int64_t>({10, 10, 10}));
    EXPECT_EQ(Values<std::string>(*batch, 1),
              std::vector<std::string>({"a", "c", "d"}));
}

TEST(TopKOperatorTest, AllowsLimitLargerThanInput) {
    TopKOperator op(MakeInt64ValueInput({{4, 1}, {3}}),
                    {SortKey{"value", SortOrder::ASC}}, 10);

    std::optional<ExecBatch> batch = op.Next();
    ASSERT_TRUE(batch.has_value());
    EXPECT_EQ(Values<int64_t>(*batch, 0), std::vector<int64_t>({1, 3, 4}));
    EXPECT_EQ(batch->row_count, 3u);
}

TEST(TopKOperatorTest, ZeroLimitReturnsEmptyStream) {
    TopKOperator op(MakeInt64ValueInput({{4, 1}, {3}}),
                    {SortKey{"value", SortOrder::ASC}}, 0);

    EXPECT_FALSE(op.Next().has_value());
}
