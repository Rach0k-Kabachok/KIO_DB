#include <cstdint>
#include <memory>
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

TEST(SortOperatorTest, JustWorksAcrossBatches) {
    SortOperator op(MakeInt64ValueInput({{5, 1, 8}, {3, 2}, {7}}),
                    {SortKey{"value", SortOrder::ASC}});

    std::optional<ExecBatch> batch = op.Next();
    ASSERT_TRUE(batch.has_value());
    EXPECT_EQ(Values<int64_t>(*batch, 0),
              std::vector<int64_t>({1, 2, 3, 5, 7, 8}));
    EXPECT_EQ(batch->row_count, 6u);
    EXPECT_FALSE(op.Next().has_value());
}

TEST(SortOperatorTest, SupportsDescAndTieBreakKeys) {
    auto schema = MakeSchema({{"score", "int64"}, {"name", "string"}});

    std::vector<ExecBatch> batches;
    batches.push_back(MakeScoreNameBatch(schema, {9, 10}, {"b", "c"}));
    batches.push_back(MakeScoreNameBatch(schema, {10, 8, 10}, {"a", "z", "d"}));

    SortOperator op(
        MakeInput(std::move(batches)),
        {SortKey{"score", SortOrder::DESC}, SortKey{"name", SortOrder::ASC}});

    std::optional<ExecBatch> batch = op.Next();
    ASSERT_TRUE(batch.has_value());
    EXPECT_EQ(Values<int64_t>(*batch, 0),
              std::vector<int64_t>({10, 10, 10, 9, 8}));
    EXPECT_EQ(Values<std::string>(*batch, 1),
              std::vector<std::string>({"a", "c", "d", "b", "z"}));
}

TEST(SortOperatorTest, EmptyInputReturnsEmptyStream) {
    SortOperator op(MakeInput({}), {SortKey{"value", SortOrder::ASC}});
    EXPECT_FALSE(op.Next().has_value());
}

TEST(SortOperatorTest, EmptyBatchesDoNotBreakSort) {
    SortOperator op(MakeInt64ValueInput({{}, {2, 1}, {}}),
                    {SortKey{"value", SortOrder::ASC}});

    std::optional<ExecBatch> batch = op.Next();
    ASSERT_TRUE(batch.has_value());
    EXPECT_EQ(Values<int64_t>(*batch, 0), std::vector<int64_t>({1, 2}));
    EXPECT_EQ(batch->row_count, 2u);
}
