#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "execution/operator.h"
#include "global/column_operations.h"
#include "global/columnar_types.h"
#include "global/schema.h"

namespace test_exec {

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

inline std::shared_ptr<const Schema> MakeSchema(
    std::vector<std::vector<std::string>> columns) {
    return std::make_shared<Schema>(std::move(columns));
}

inline ExecBatch MakeBatch(std::shared_ptr<const Schema> schema,
                           ctp::ColumnarBatch columns) {
    const size_t row_count =
        columns.empty() ? 0 : ctp::GetColumnRowCount(columns[0]);
    return ExecBatch{std::move(columns), std::move(schema), row_count};
}

inline std::unique_ptr<IOperator> MakeInput(std::vector<ExecBatch> batches) {
    return std::make_unique<VectorOperator>(std::move(batches));
}

template <typename T>
const std::vector<T>& Values(const ExecBatch& batch, size_t column_idx) {
    return std::get<std::vector<T>>(batch.columns[column_idx]);
}

template <typename T>
std::vector<T> ReadAllValues(IOperator& op, size_t column_idx = 0) {
    std::vector<T> result;
    while (std::optional<ExecBatch> batch = op.Next()) {
        const auto& values = Values<T>(*batch, column_idx);
        result.insert(result.end(), values.begin(), values.end());
    }
    return result;
}

inline std::unique_ptr<IOperator> MakeInt64ValueInput(
    const std::vector<std::vector<int64_t>>& batches,
    const std::string& column_name = "value") {
    auto schema = MakeSchema({{column_name, "int64"}});
    std::vector<ExecBatch> exec_batches;
    exec_batches.reserve(batches.size());

    for (std::vector<int64_t> values : batches) {
        exec_batches.push_back(MakeBatch(
            schema, ctp::ColumnarBatch{ctp::Column{std::move(values)}}));
    }

    return MakeInput(std::move(exec_batches));
}

inline ExecBatch MakeScoreNameBatch(std::shared_ptr<const Schema> schema,
                                    std::vector<int64_t> scores,
                                    std::vector<std::string> names) {
    return MakeBatch(
        std::move(schema),
        ctp::ColumnarBatch{ctp::Column{std::move(scores)},
                           ctp::Column{std::move(names)}});
}

}  // namespace test_exec
