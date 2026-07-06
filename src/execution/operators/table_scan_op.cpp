#include "execution/operators/table_scan_op.h"

#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "transport/csv/csv_type_parser.h"
#include "transport/kio/kio_db_reader.h"

namespace {

scalar::Value ParseMinMaxValue(const std::string& value, Schema::Types type) {
    switch (type) {
    case Schema::BIGINT:
    case Schema::TIMESTAMP:
        return csv::ParseNum<int64_t>(value);
    case Schema::INTEGER:
    case Schema::DATE:
        return csv::ParseNum<int32_t>(value);
    case Schema::SMALLINT:
        return csv::ParseNum<int16_t>(value);
    case Schema::DOUBLE:
        return csv::ParseNum<double>(value);
    case Schema::TEXT:
    case Schema::VARCHAR:
        return value;
    case Schema::CHAR:
        return value.empty() ? '\0' : value[0];
    }

    throw std::invalid_argument("Unsupported min/max type");
}

bool ConstraintMayMatch(const kio::ColumnChunkInfo& chunk,
                        const MinMaxConstraint& constraint) {
    if (!chunk.has_min_max) {
        return true;
    }

    const scalar::Value min_value =
        ParseMinMaxValue(chunk.min_value, constraint.type);
    const scalar::Value max_value =
        ParseMinMaxValue(chunk.max_value, constraint.type);

    if (constraint.lower.has_value()) {
        const int cmp = scalar::Compare(max_value, *constraint.lower);
        if (cmp < 0 || (cmp == 0 && !constraint.lower_inclusive)) {
            return false;
        }
    }

    if (constraint.upper.has_value()) {
        const int cmp = scalar::Compare(min_value, *constraint.upper);
        if (cmp > 0 || (cmp == 0 && !constraint.upper_inclusive)) {
            return false;
        }
    }

    if (constraint.not_equal &&
        scalar::Compare(min_value, constraint.not_equal_value) == 0 &&
        scalar::Compare(max_value, constraint.not_equal_value) == 0) {
        return false;
    }

    return true;
}

bool RowGroupMayMatch(
    const Schema& schema,
    const kio::RowGroupMeta& row_group,
    const std::shared_ptr<std::vector<MinMaxConstraint>>& constraints) {
    if (!constraints || constraints->empty()) {
        return true;
    }

    for (const MinMaxConstraint& constraint : *constraints) {
        const size_t column_idx = schema.ColumnIndex(constraint.column_name);
        if (!ConstraintMayMatch(row_group.columns[column_idx], constraint)) {
            return false;
        }
    }

    return true;
}

}  // namespace

TableScanOperator::TableScanOperator(
        const std::string& db_filename,
        const std::vector<std::string>& column_names)
        : TableScanOperator(db_filename, column_names, nullptr) {
}

TableScanOperator::TableScanOperator(
        const std::string& db_filename,
        const std::vector<std::string>& column_names,
        std::shared_ptr<std::vector<MinMaxConstraint>> constraints)
        : reader_(db_filename) {
    const Schema& file_schema = reader_.GetSchema();
    column_indices_.reserve(column_names.size());
    constraints_ = std::move(constraints);

    for (const auto& name : column_names) {
        column_indices_.push_back(file_schema.ColumnIndex(name));
    }

    output_schema_ =
        std::make_shared<Schema>(file_schema.ProjectByIndices(column_indices_));
}


std::optional<ExecBatch> TableScanOperator::Next() {
    const kio::RowGroupMeta* row_group;
    while ((row_group = reader_.PeekNextRowGroupMeta())) {
        if (RowGroupMayMatch(reader_.GetSchema(),
                            *row_group, constraints_)) {
            break;
        }
        reader_.SkipNextBatch();
    }

    if (row_group == nullptr) {
        return std::nullopt;
    }

    const size_t row_count = row_group->batch.row_num;

    ctp::ColumnarBatch columns = reader_.ReadNextBatch(column_indices_).value();

    return ExecBatch{std::move(columns), output_schema_, row_count};
}
