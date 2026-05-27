#include <cstddef>
#include <cstdint>
#include <map>
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
using test_exec::MakeSchema;
using test_exec::Values;

std::shared_ptr<const Schema> MakeAggregateSchema() {
    return MakeSchema({{"grp", "string"},
                       {"sub", "int64"},
                       {"value", "int64"},
                       {"name", "string"}});
}

ExecBatch MakeAggregateBatch(std::shared_ptr<const Schema> schema,
                             std::vector<std::string> groups,
                             std::vector<int64_t> subs,
                             std::vector<int64_t> values,
                             std::vector<std::string> names) {
    return MakeBatch(
        std::move(schema),
        ctp::ColumnarBatch{ctp::Column{std::move(groups)},
                           ctp::Column{std::move(subs)},
                           ctp::Column{std::move(values)},
                           ctp::Column{std::move(names)}});
}

}  // namespace

TEST(GlobalAgrOperatorTest, JustWorksAcrossBatches) {
    auto schema = MakeAggregateSchema();
    std::vector<ExecBatch> batches;
    batches.push_back(MakeAggregateBatch(
        schema, {"a", "b", "a"}, {1, 1, 2}, {1, 2, 3},
        {"b", "a", "b"}));
    batches.push_back(MakeAggregateBatch(
        schema, {"b", "c"}, {1, 1}, {2, 5}, {"a", "c"}));

    GlobalAgrOperator op(
        MakeInput(std::move(batches)),
        {{AggregateKind::COUNT, "", "cnt"},
         {AggregateKind::SUM, "value", "sum"},
         {AggregateKind::AVG, "value", "avg"},
         {AggregateKind::MIN, "name", "min_name"},
         {AggregateKind::MAX, "value", "max_value"},
         {AggregateKind::COUNT_DISTINCT, "name", "unique_names"}});

    std::optional<ExecBatch> batch = op.Next();
    ASSERT_TRUE(batch.has_value());
    EXPECT_EQ(batch->row_count, 1u);
    EXPECT_EQ(batch->schema->ColumnName(0), "cnt");
    EXPECT_EQ(batch->schema->ColumnType(2), Schema::DOUBLE);
    EXPECT_EQ(batch->schema->ColumnType(3), Schema::TEXT);

    EXPECT_EQ(Values<int64_t>(*batch, 0)[0], 5);
    EXPECT_EQ(Values<int64_t>(*batch, 1)[0], 13);
    EXPECT_DOUBLE_EQ(Values<double>(*batch, 2)[0], 2.6);
    EXPECT_EQ(Values<std::string>(*batch, 3)[0], "a");
    EXPECT_EQ(Values<int64_t>(*batch, 4)[0], 5);
    EXPECT_EQ(Values<int64_t>(*batch, 5)[0], 3);
    EXPECT_FALSE(op.Next().has_value());
}

TEST(GlobalAgrOperatorTest, EmptyInputReturnsEmptyStream) {
    GlobalAgrOperator op(MakeInput({}), {{AggregateKind::COUNT, "", "cnt"}});
    EXPECT_FALSE(op.Next().has_value());
}

TEST(GroupAgrOperatorTest, JustWorksForSingleAndCompositeKeys) {
    auto schema = MakeAggregateSchema();
    std::vector<ExecBatch> batches;
    batches.push_back(MakeAggregateBatch(
        schema, {"a", "a", "a"}, {1, 1, 2}, {10, 20, 30},
        {"x", "y", "x"}));
    batches.push_back(MakeAggregateBatch(
        schema, {"b", "a"}, {1, 1}, {40, 50}, {"z", "x"}));

    GroupAgrOperator op(
        MakeInput(std::move(batches)),
        {"grp", "sub"},
        {{AggregateKind::COUNT_DISTINCT, "name", "unique_names"},
         {AggregateKind::MAX, "value", "max_value"}});

    std::optional<ExecBatch> batch = op.Next();
    ASSERT_TRUE(batch.has_value());
    EXPECT_EQ(batch->row_count, 3u);
    EXPECT_EQ(batch->schema->ColumnName(0), "grp");
    EXPECT_EQ(batch->schema->ColumnName(2), "unique_names");

    std::map<std::pair<std::string, int64_t>, std::pair<int64_t, int64_t>>
        result;
    for (size_t idx = 0; idx < batch->row_count; idx++) {
        result[{Values<std::string>(*batch, 0)[idx],
                Values<int64_t>(*batch, 1)[idx]}] =
            {Values<int64_t>(*batch, 2)[idx],
             Values<int64_t>(*batch, 3)[idx]};
    }

    using GroupPair = std::pair<std::string, int64_t>;
    using IntPair = std::pair<int64_t, int64_t>;
    EXPECT_EQ(result[GroupPair("a", 1)], IntPair(2, 50));
    EXPECT_EQ(result[GroupPair("a", 2)], IntPair(1, 30));
    EXPECT_EQ(result[GroupPair("b", 1)], IntPair(1, 40));
    EXPECT_FALSE(op.Next().has_value());
}
