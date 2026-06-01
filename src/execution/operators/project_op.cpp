#include "execution/operators/project_op.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "global/columnar_types.h"

ProjectOperator::ProjectOperator(std::unique_ptr<IOperator> child_op,
                    std::vector<std::string> column_names):
                    child_op_(std::move(child_op)),
                    column_names_(std::move(column_names)) {
}

std::optional<ExecBatch> ProjectOperator::Next() {
    std::optional<ExecBatch> optional_batch = child_op_->Next();

    if (!optional_batch.has_value()) {
        return std::nullopt;
    }

    ExecBatch& exec_batch = optional_batch.value();
    if (!output_schema_) {
        column_indices_.reserve(column_names_.size());
        for (const std::string& name : column_names_) {
            column_indices_.push_back(exec_batch.schema->ColumnIndex(name));
        }
        output_schema_ =
            std::make_shared<Schema>(
                exec_batch.schema->ProjectByIndices(column_indices_));
    }

    ctp::ColumnarBatch new_batch;
    new_batch.reserve(column_indices_.size());

    for (size_t index : column_indices_) {
        new_batch.push_back(std::move(exec_batch.columns[index]));
    }

    return ExecBatch{std::move(new_batch), output_schema_,
                     exec_batch.row_count};
}
