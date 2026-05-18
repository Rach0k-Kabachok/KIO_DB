#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
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

ExecBatch MakeValueBatch(std::shared_ptr<const Schema> schema,
                         std::vector<int64_t> values) {
    const size_t row_count = values.size();
    return ExecBatch{ctp::ColumnarBatch{ctp::Column{std::move(values)}},
                     std::move(schema), row_count};
}

ExecBatch MakeScoreNameBatch(std::shared_ptr<const Schema> schema,
                             std::vector<int64_t> scores,
                             std::vector<std::string> names) {
    const size_t row_count = scores.size();
    return ExecBatch{ctp::ColumnarBatch{ctp::Column{std::move(scores)},
                                        ctp::Column{std::move(names)}},
                     std::move(schema), row_count};
}

std::unique_ptr<IOperator> MakeValueInput(
    const std::vector<std::vector<int64_t>>& batches) {
    auto schema = std::make_shared<Schema>(
        std::vector<std::vector<std::string>>{{"value", "int64"}});

    std::vector<ExecBatch> exec_batches;
    exec_batches.reserve(batches.size());
    for (std::vector<int64_t> values : batches) {
        exec_batches.push_back(MakeValueBatch(schema, std::move(values)));
    }

    return std::make_unique<VectorOperator>(std::move(exec_batches));
}
}  // namespace

TEST(SortOperatorTest, SortsAscAcrossBatches) {
    SortOperator op(MakeValueInput({{5, 1, 8}, {3, 2}, {7}}),
                    {SortKey{"value", SortOrder::ASC}});

    std::optional<ExecBatch> batch = op.Next();
    ASSERT_TRUE(batch.has_value());
    EXPECT_EQ(std::get<std::vector<int64_t>>(batch->columns[0]),
              std::vector<int64_t>({1, 2, 3, 5, 7, 8}));
    EXPECT_EQ(batch->row_count, 6u);
    EXPECT_FALSE(op.Next().has_value());
}

TEST(SortOperatorTest, SortsDescAcrossBatches) {
    SortOperator op(MakeValueInput({{5, 1, 8}, {3, 2}, {7}}),
                    {SortKey{"value", SortOrder::DESC}});

    std::optional<ExecBatch> batch = op.Next();
    ASSERT_TRUE(batch.has_value());
    EXPECT_EQ(std::get<std::vector<int64_t>>(batch->columns[0]),
              std::vector<int64_t>({8, 7, 5, 3, 2, 1}));
    EXPECT_EQ(batch->row_count, 6u);
}

TEST(SortOperatorTest, SortsByTwoKeys) {
    auto schema =
        std::make_shared<Schema>(std::vector<std::vector<std::string>>{
            {"score", "int64"}, {"name", "string"}});

    std::vector<ExecBatch> batches;
    batches.push_back(MakeScoreNameBatch(schema, {9, 10}, {"b", "c"}));
    batches.push_back(MakeScoreNameBatch(schema, {10, 8, 10}, {"a", "z", "d"}));

    SortOperator op(
        std::make_unique<VectorOperator>(std::move(batches)),
        {SortKey{"score", SortOrder::DESC}, SortKey{"name", SortOrder::ASC}});

    std::optional<ExecBatch> batch = op.Next();
    ASSERT_TRUE(batch.has_value());
    EXPECT_EQ(std::get<std::vector<int64_t>>(batch->columns[0]),
              std::vector<int64_t>({10, 10, 10, 9, 8}));
    EXPECT_EQ(std::get<std::vector<std::string>>(batch->columns[1]),
              std::vector<std::string>({"a", "c", "d", "b", "z"}));
    EXPECT_EQ(batch->row_count, 5u);
}

TEST(SortOperatorTest, EmptyInputReturnsEmptyStream) {
    SortOperator op(std::make_unique<VectorOperator>(std::vector<ExecBatch>{}),
                    {SortKey{"value", SortOrder::ASC}});

    EXPECT_FALSE(op.Next().has_value());
}

TEST(SortOperatorTest, EmptyBatchesDoNotBreakSort) {
    auto schema = std::make_shared<Schema>(
        std::vector<std::vector<std::string>>{{"value", "int64"}});

    std::vector<ExecBatch> batches;
    batches.push_back(MakeValueBatch(schema, {}));
    batches.push_back(MakeValueBatch(schema, {2, 1}));
    batches.push_back(MakeValueBatch(schema, {}));

    SortOperator op(std::make_unique<VectorOperator>(std::move(batches)),
                    {SortKey{"value", SortOrder::ASC}});

    std::optional<ExecBatch> batch = op.Next();
    ASSERT_TRUE(batch.has_value());
    EXPECT_EQ(std::get<std::vector<int64_t>>(batch->columns[0]),
              std::vector<int64_t>({1, 2}));
    EXPECT_EQ(batch->row_count, 2u);
}
