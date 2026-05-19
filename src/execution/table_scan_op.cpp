#include "execution/operators.h"

#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "transport/csv/csv_type_parser.h"
#include "transport/kio/kio_db_reader.h"

namespace {

VarType ParseMinMaxValue(const std::string& value, Schema::Types type) {
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

int CompareValues(const VarType& lhs, const VarType& rhs) {
    return std::visit([&rhs](const auto& lhs_value) {
        using Value = std::decay_t<decltype(lhs_value)>;
        const Value& rhs_value = std::get<Value>(rhs);
        if (lhs_value == rhs_value) {
            return 0;
        }
        return lhs_value < rhs_value ? -1 : 1;
    }, lhs);
}

bool ConstraintMayMatch(const kio::ColumnChunkInfo& chunk,
                        const MinMaxConstraint& constraint) {
    if (!chunk.has_min_max) {
        return true;
    }

    const VarType min_value =
        ParseMinMaxValue(chunk.min_value, constraint.type);
    const VarType max_value =
        ParseMinMaxValue(chunk.max_value, constraint.type);

    if (constraint.lower.has_value()) {
        const int cmp = CompareValues(max_value, *constraint.lower);
        if (cmp < 0 || (cmp == 0 && !constraint.lower_inclusive)) {
            return false;
        }
    }

    if (constraint.upper.has_value()) {
        const int cmp = CompareValues(min_value, *constraint.upper);
        if (cmp > 0 || (cmp == 0 && !constraint.upper_inclusive)) {
            return false;
        }
    }

    if (constraint.not_equal &&
        CompareValues(min_value, constraint.not_equal_value) == 0 &&
        CompareValues(max_value, constraint.not_equal_value) == 0) {
        return false;
    }

    return true;
}

bool RowGroupMayMatch(
        const Schema& schema,
        const kio::RowGroupMeta& row_group,
        const std::vector<MinMaxConstraint>& constraints) {
    for (const MinMaxConstraint& constraint : constraints) {
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
    if (constraints_) {
        const auto& row_groups = reader_.GetMetadata().row_groups;
        while (reader_.GetNextRowGroupIndex() < row_groups.size()) {
            const size_t group_idx = reader_.GetNextRowGroupIndex();
            if (RowGroupMayMatch(reader_.GetSchema(),
                                 reader_.GetRowGroupMeta(group_idx),
                                 *constraints_)) {
                break;
            }
            reader_.SkipNextBatch();
        }
    }
    
    std::optional<KioReadBatch> batch = reader_.ReadNextBatch(column_indices_);
    if (!batch.has_value()) {
        return std::nullopt;
    }

    return ExecBatch{std::move(batch->columns), output_schema_, batch->row_count};
}
