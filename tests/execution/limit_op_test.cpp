#include <memory>
#include <optional>
#include <cstddef>
#include <string>
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

ExecBatch MakeBatch(std::vector<int64_t> values) {
    const size_t row_count = values.size();
    auto schema = std::make_shared<Schema>(
        std::vector<std::vector<std::string>>{{"value", "int64"}});
    return ExecBatch{
        ctp::ColumnarBatch{ctp::Column{std::move(values)}},
        std::move(schema),
        row_count};
}

ExecBatch MakeTwoColumnBatch(std::shared_ptr<const Schema> schema,
                             std::vector<std::string> names,
                             std::vector<int64_t> values) {
    const size_t row_count = values.size();
    return ExecBatch{
        ctp::ColumnarBatch{
            ctp::Column{std::move(names)},
            ctp::Column{std::move(values)}},
        std::move(schema),
        row_count};
}

std::unique_ptr<IOperator> MakeInput() {
    std::vector<ExecBatch> batches;
    batches.push_back(MakeBatch({1, 2, 3}));
    batches.push_back(MakeBatch({4, 5, 6}));
    batches.push_back(MakeBatch({7, 8}));
    return std::make_unique<VectorOperator>(std::move(batches));
}

std::vector<int64_t> ReadAllValues(IOperator& op) {
    std::vector<int64_t> result;
    while (std::optional<ExecBatch> batch = op.Next()) {
        const auto& values = std::get<std::vector<int64_t>>(batch->columns[0]);
        result.insert(result.end(), values.begin(), values.end());
    }
    return result;
}
}  // namespace

TEST(LimitOperatorTest, AppliesLimit) {
    LimitOperator op(MakeInput(), 4);
    EXPECT_EQ(ReadAllValues(op), std::vector<int64_t>({1, 2, 3, 4}));
}

TEST(LimitOperatorTest, AppliesOffset) {
    LimitOperator op(MakeInput(), 10, 5);
    EXPECT_EQ(ReadAllValues(op), std::vector<int64_t>({6, 7, 8}));
}

TEST(LimitOperatorTest, AppliesLimitAndOffsetAcrossBatches) {
    LimitOperator op(MakeInput(), 3, 2);
    EXPECT_EQ(ReadAllValues(op), std::vector<int64_t>({3, 4, 5}));
}

TEST(LimitOperatorTest, EmptyWhenOffsetIsPastInput) {
    LimitOperator op(MakeInput(), 3, 100);
    EXPECT_TRUE(ReadAllValues(op).empty());
}

TEST(FilterOperatorTest, KeepsInputSchemaPointer) {
    auto schema = std::make_shared<Schema>(
        std::vector<std::vector<std::string>>{{"name", "string"},
                                              {"value", "int64"}});

    std::vector<ExecBatch> batches;
    batches.push_back(MakeTwoColumnBatch(
        schema, {"a", "b", "c"}, {10, 20, 30}));

    FilterOperator op(
        std::make_unique<VectorOperator>(std::move(batches)),
        [](const ExecBatch& batch, size_t row_idx) {
            const auto& values =
                std::get<std::vector<int64_t>>(batch.columns[1]);
            return values[row_idx] >= 20;
        });

    std::optional<ExecBatch> batch = op.Next();
    ASSERT_TRUE(batch.has_value());
    EXPECT_EQ(batch->schema, schema);
    EXPECT_EQ(std::get<std::vector<std::string>>(batch->columns[0]),
              std::vector<std::string>({"b", "c"}));
    EXPECT_EQ(std::get<std::vector<int64_t>>(batch->columns[1]),
              std::vector<int64_t>({20, 30}));
}

TEST(ProjectOperatorTest, ReusesProjectedSchema) {
    auto input_schema = std::make_shared<Schema>(
        std::vector<std::vector<std::string>>{{"name", "string"},
                                              {"value", "int64"}});

    std::vector<ExecBatch> batches;
    batches.push_back(MakeTwoColumnBatch(input_schema, {"a", "b"}, {10, 20}));
    batches.push_back(MakeTwoColumnBatch(input_schema, {"c"}, {30}));

    ProjectOperator op(
        std::make_unique<VectorOperator>(std::move(batches)), {"value"});

    std::optional<ExecBatch> first = op.Next();
    std::optional<ExecBatch> second = op.Next();

    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(first->schema, second->schema);
    EXPECT_EQ(first->schema->ColumnCount(), 1u);
    EXPECT_EQ(first->schema->ColumnName(0), "value");
    EXPECT_EQ(first->schema->ColumnType(0), Schema::BIGINT);
    EXPECT_EQ(std::get<std::vector<int64_t>>(first->columns[0]),
              std::vector<int64_t>({10, 20}));
    EXPECT_EQ(std::get<std::vector<int64_t>>(second->columns[0]),
              std::vector<int64_t>({30}));
}
