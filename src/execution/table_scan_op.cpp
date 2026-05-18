#include "execution/operators.h"

#include <memory>
#include <utility>
#include <vector>

#include "transport/kio/kio_db_reader.h"

TableScanOperator::TableScanOperator(
        const std::string& db_filename,
        const std::vector<std::string>& column_names)
        : reader_(db_filename) {
    const Schema& file_schema = reader_.GetSchema();
    column_indices_.reserve(column_names.size());

    for (const auto& name : column_names) {
        column_indices_.push_back(file_schema.ColumnIndex(name));
    }

    output_schema_ =
        std::make_shared<Schema>(file_schema.ProjectByIndices(column_indices_));
}


std::optional<ExecBatch> TableScanOperator::Next() {
    
    std::optional<KioReadBatch> batch = reader_.ReadNextBatch(column_indices_);
    if (!batch.has_value()) {
        return std::nullopt;
    }

    return ExecBatch{std::move(batch->columns), output_schema_, batch->row_count};
}
