#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "gtest/gtest.h"

#include "execution/operators.h"
#include "global/columnar_types.h"
#include "global/schema.h"

namespace {
class VectorOperator final : public IOperator {
public:
    explicit VectorOperator(std::vector<ExecBatch> batches)
        : batches_(std::move(batches)) {
    }

    std::optional<ExecBatch> Next() override {
        if (next_ >= batches_.size()) {
            return std::nullopt;
        }
        return std::move(batches_[next_++]);
    }

private:
    std::vector<ExecBatch> batches_;
    size_t next_ = 0;
};

std::shared_ptr<const Schema> MakeAggregateTestSchema() {
    return std::make_shared<Schema>(
        std::vector<std::vector<std::string>>{
            {"grp", "string"},
            {"sub", "int64"},
            {"value", "int64"},
            {"name", "string"}});
}

ExecBatch MakeAggregateTestBatch(
        std::shared_ptr<const Schema> schema,
        std::vector<std::string> groups,
        std::vector<int64_t> subs,
        std::vector<int64_t> values,
        std::vector<std::string> names) {
    const size_t row_count = values.size();
    return ExecBatch{
        ctp::ColumnarBatch{
            ctp::Column{std::move(groups)},
            ctp::Column{std::move(subs)},
            ctp::Column{std::move(values)},
            ctp::Column{std::move(names)}},
        std::move(schema),
        row_count};
}
}  // namespace

TEST(GlobalAgrOperatorTest, AggregatesAcrossBatches) {
    auto schema = MakeAggregateTestSchema();
    std::vector<ExecBatch> batches;
    batches.push_back(MakeAggregateTestBatch(
        schema, {"a", "b", "a"}, {1, 1, 2}, {1, 2, 3},
        {"b", "a", "b"}));
    batches.push_back(MakeAggregateTestBatch(
        schema, {"b", "c"}, {1, 1}, {2, 5}, {"a", "c"}));

    GlobalAgrOperator op(
        std::make_unique<VectorOperator>(std::move(batches)),
        std::vector<AggregateSpec>{
            {AggregateKind::COUNT, "", "cnt"},
            {AggregateKind::SUM, "value", "sum"},
            {AggregateKind::AVG, "value", "avg"},
            {AggregateKind::MIN, "name", "min_name"},
            {AggregateKind::MAX, "value", "max_value"},
            {AggregateKind::COUNT_DISTINCT, "name", "unique_names"}});

    std::optional<ExecBatch> batch = op.Next();
    ASSERT_TRUE(batch.has_value());
    EXPECT_EQ(batch->row_count, 1u);
    EXPECT_EQ(batch->schema->ColumnName(0), "cnt");
    EXPECT_EQ(batch->schema->ColumnName(3), "min_name");
    EXPECT_EQ(batch->schema->ColumnType(3), Schema::TEXT);

    EXPECT_EQ(std::get<std::vector<int64_t>>(batch->columns[0])[0], 5);
    EXPECT_EQ(std::get<std::vector<int64_t>>(batch->columns[1])[0], 13);
    EXPECT_EQ(std::get<std::vector<int64_t>>(batch->columns[2])[0], 2);
    EXPECT_EQ(std::get<std::vector<std::string>>(batch->columns[3])[0], "a");
    EXPECT_EQ(std::get<std::vector<int64_t>>(batch->columns[4])[0], 5);
    EXPECT_EQ(std::get<std::vector<int64_t>>(batch->columns[5])[0], 3);
    EXPECT_FALSE(op.Next().has_value());
}

TEST(GroupAgrOperatorTest, GroupsByOneColumnAcrossBatches) {
    auto schema = MakeAggregateTestSchema();
    std::vector<ExecBatch> batches;
    batches.push_back(MakeAggregateTestBatch(
        schema, {"a", "b", "a"}, {1, 1, 1}, {1, 2, 3},
        {"x", "y", "x"}));
    batches.push_back(MakeAggregateTestBatch(
        schema, {"b", "c"}, {1, 1}, {4, 5}, {"z", "q"}));

    GroupAgrOperator op(
        std::make_unique<VectorOperator>(std::move(batches)),
        std::vector<std::string>{"grp"},
        std::vector<AggregateSpec>{
            {AggregateKind::COUNT, "", "cnt"},
            {AggregateKind::SUM, "value", "sum"}});

    std::optional<ExecBatch> batch = op.Next();
    ASSERT_TRUE(batch.has_value());
    EXPECT_EQ(batch->row_count, 3u);
    EXPECT_EQ(batch->schema->ColumnName(0), "grp");
    EXPECT_EQ(batch->schema->ColumnName(1), "cnt");
    EXPECT_EQ(batch->schema->ColumnName(2), "sum");

    const auto& groups = std::get<std::vector<std::string>>(batch->columns[0]);
    const auto& counts = std::get<std::vector<int64_t>>(batch->columns[1]);
    const auto& sums = std::get<std::vector<int64_t>>(batch->columns[2]);

    std::map<std::string, std::pair<int64_t, int64_t>> result;
    for (size_t idx = 0; idx < batch->row_count; idx++) {
        result[groups[idx]] = {counts[idx], sums[idx]};
    }

    using IntPair = std::pair<int64_t, int64_t>;
    EXPECT_EQ(result["a"], IntPair(2, 4));
    EXPECT_EQ(result["b"], IntPair(2, 6));
    EXPECT_EQ(result["c"], IntPair(1, 5));
    EXPECT_FALSE(op.Next().has_value());
}

TEST(GroupAgrOperatorTest, GroupsByTwoColumnsAndCountsDistinct) {
    auto schema = MakeAggregateTestSchema();
    std::vector<ExecBatch> batches;
    batches.push_back(MakeAggregateTestBatch(
        schema, {"a", "a", "a"}, {1, 1, 2}, {10, 20, 30},
        {"x", "y", "x"}));
    batches.push_back(MakeAggregateTestBatch(
        schema, {"b", "a"}, {1, 1}, {40, 50}, {"z", "x"}));

    GroupAgrOperator op(
        std::make_unique<VectorOperator>(std::move(batches)),
        std::vector<std::string>{"grp", "sub"},
        std::vector<AggregateSpec>{
            {AggregateKind::COUNT_DISTINCT, "name", "unique_names"},
            {AggregateKind::MAX, "value", "max_value"}});

    std::optional<ExecBatch> batch = op.Next();
    ASSERT_TRUE(batch.has_value());
    EXPECT_EQ(batch->row_count, 3u);
    EXPECT_EQ(batch->schema->ColumnName(0), "grp");
    EXPECT_EQ(batch->schema->ColumnName(1), "sub");
    EXPECT_EQ(batch->schema->ColumnName(2), "unique_names");
    EXPECT_EQ(batch->schema->ColumnName(3), "max_value");

    const auto& groups = std::get<std::vector<std::string>>(batch->columns[0]);
    const auto& subs = std::get<std::vector<int64_t>>(batch->columns[1]);
    const auto& distincts = std::get<std::vector<int64_t>>(batch->columns[2]);
    const auto& max_values = std::get<std::vector<int64_t>>(batch->columns[3]);

    std::map<std::pair<std::string, int64_t>, std::pair<int64_t, int64_t>>
        result;
    for (size_t idx = 0; idx < batch->row_count; idx++) {
        result[{groups[idx], subs[idx]}] = {distincts[idx], max_values[idx]};
    }

    using GroupPair = std::pair<std::string, int64_t>;
    using IntPair = std::pair<int64_t, int64_t>;
    EXPECT_EQ(result[GroupPair("a", 1)], IntPair(2, 50));
    EXPECT_EQ(result[GroupPair("a", 2)], IntPair(1, 30));
    EXPECT_EQ(result[GroupPair("b", 1)], IntPair(1, 40));
    EXPECT_FALSE(op.Next().has_value());
}
