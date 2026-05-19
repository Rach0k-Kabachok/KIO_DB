#include "execution/operators.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "global/column_operations.h"
#include "global/columnar_types.h"
#include "global/scalar_value.h"
#include "global/schema.h"

ComputeOperator::ComputeOperator(
        std::unique_ptr<IOperator> child_op,
        std::vector<ComputedColumnSpec> computed_columns)
        : child_op_(std::move(child_op)),
          computed_columns_(std::move(computed_columns)) {
}

std::optional<ExecBatch> ComputeOperator::Next() {
    std::optional<ExecBatch> optional_batch = child_op_->Next();
    if (!optional_batch.has_value()) {
        return std::nullopt;
    }

    ExecBatch& batch = optional_batch.value();
    if (!output_schema_) {
        std::vector<std::string> names;
        std::vector<Schema::Types> types;
        names.reserve(batch.schema->ColumnCount() + computed_columns_.size());
        types.reserve(batch.schema->ColumnCount() + computed_columns_.size());

        for (size_t idx = 0; idx < batch.schema->ColumnCount(); idx++) {
            names.push_back(batch.schema->ColumnName(idx));
            types.push_back(batch.schema->ColumnType(idx));
        }

        for (const ComputedColumnSpec& column : computed_columns_) {
            names.push_back(column.name);
            types.push_back(column.type);
        }

        output_schema_ = std::make_shared<Schema>(
            Schema::FromColumns(std::move(names), std::move(types)));
    }

    batch.columns.reserve(batch.columns.size() + computed_columns_.size());
    for (const ComputedColumnSpec& spec : computed_columns_) {
        ctp::Column column = ctp::MakeEmptyColumn(spec.type, batch.row_count);
        for (size_t row_idx = 0; row_idx < batch.row_count; row_idx++) {
            scalar::AppendValue(column, spec.compute(batch, row_idx));
        }
        batch.columns.push_back(std::move(column));
    }

    return ExecBatch{std::move(batch.columns), output_schema_, batch.row_count};
}
