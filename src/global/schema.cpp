#include "schema.h"

#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "transport/csv/csv_batch_reader.h"


Schema::Schema(
        const std::vector<std::vector<std::string>>& column_names_types) {
    ResetFromRows(column_names_types);
}

Schema::Schema(const std::string& schema_file) {
    LoadFromCsv(schema_file);
}


Schema::Types Schema::ParseType(const std::string& type) {
    if (type == "BIGINT" || type == "int64") {
        return BIGINT;
    } else if (type == "INTEGER" || type == "INTENGER") {
        return INTEGER;
    } else if (type == "SMALLINT") {
        return SMALLINT;
    } else if (type == "TEXT" || type == "string") {
        return TEXT;
    } else if (type == "VARCHAR" || type == "VARCHAR(255)") {
        return VARCHAR;
    } else if (type == "CHAR") {
        return CHAR;
    } else if (type == "TIMESTAMP") {
        return TIMESTAMP;
    } else if (type == "DATE") {
        return DATE;
    } else if (type == "DOUBLE" || type == "double" || type == "Float64") {
        return DOUBLE;
    }

    throw std::invalid_argument("Invalid schema type: " + type);
}

std::string Schema::TypeToString(Types type) {
    switch (type) {
    case BIGINT:
        return "BIGINT";
    case INTEGER:
        return "INTEGER";
    case SMALLINT:
        return "SMALLINT";
    case TEXT:
        return "TEXT";
    case VARCHAR:
        return "VARCHAR(255)";
    case CHAR:
        return "CHAR";
    case TIMESTAMP:
        return "TIMESTAMP";
    case DATE:
        return "DATE";
    case DOUBLE:
        return "DOUBLE";
    }

    throw std::invalid_argument("Invalid schema type");
}

Schema Schema::FromRows(
        const std::vector<std::vector<std::string>>& column_names_types) {
    return Schema(column_names_types);
}

Schema Schema::FromColumns(std::vector<std::string> column_names,
                           std::vector<Types> column_types) {
    Schema schema;
    schema.ResetFromColumns(std::move(column_names), std::move(column_types));
    return schema;
}

void Schema::ResetFromRows(
        const std::vector<std::vector<std::string>>& column_names_types) {
    Clear();

    if (column_names_types.empty()) {
        return;
    }

    for (const auto& row : column_names_types) {
        if (row.size() != 2) {
            throw std::invalid_argument(
                "Invalid schema row size (expected 2 columns: [name, "
                "type])");
        }

        const std::string& col_name = row[0];
        const std::string& col_type = row[1];

        if (names_to_index_.contains(col_name)) {
            throw std::invalid_argument(
                "Duplicate column name in schema: " + col_name);
        }

        Types type = ParseType(col_type);

        names_to_index_[col_name] = index_to_names_.size();
        index_to_names_.push_back(col_name);
        index_to_types_.push_back(type);
    }
}

void Schema::ResetFromColumns(std::vector<std::string> column_names,
                              std::vector<Types> column_types) {
    Clear();

    if (column_names.size() != column_types.size()) {
        throw std::invalid_argument(
            "Schema column names and types have different sizes");
    }

    names_to_index_.reserve(column_names.size());
    index_to_names_.reserve(column_names.size());
    index_to_types_.reserve(column_types.size());

    for (size_t i = 0; i < column_names.size(); ++i) {
        if (names_to_index_.contains(column_names[i])) {
            throw std::invalid_argument(
                "Duplicate column name in schema: " + column_names[i]);
        }

        names_to_index_[column_names[i]] = i;
        index_to_names_.push_back(std::move(column_names[i]));
        index_to_types_.push_back(column_types[i]);
    }
}

void Schema::LoadFromCsv(const std::string& schema_file) {
    CSVBatchReader batch_reader(schema_file);
    ctp::ParsedBatch schema;
    ctp::ParsedBatch cur_batch;

    while (!(cur_batch = batch_reader.ParseNextBatch()).empty()) {
        if (schema.empty()) {
            schema = std::move(cur_batch);
        } else {
            schema.insert(schema.end(), cur_batch.begin(), cur_batch.end());
        }
    }

    ResetFromRows(schema);
}

void Schema::Clear() {
    names_to_index_.clear();
    index_to_types_.clear();
    index_to_names_.clear();
}

size_t Schema::ColumnIndex(const std::string& name) const {
    auto it = names_to_index_.find(name);
    if (it == names_to_index_.end()) {
        throw std::out_of_range("Column not found in schema: " + name);
    }
    return it->second;
}


Schema::Types Schema::ColumnType(size_t index) const {
    if (index >= index_to_types_.size()) {
        throw std::out_of_range("Column index out of range");
    }
    return index_to_types_[index];
}

Schema::Types Schema::ColumnType(const std::string& name) const {
    return ColumnType(ColumnIndex(name));
}



const std::string& Schema::ColumnName(size_t index) const {
    if (index >= index_to_names_.size()) {
        throw std::out_of_range("Column index out of range: " +
                                std::to_string(index));
    }
    return index_to_names_[index];
}


size_t Schema::ColumnCount() const {
    return index_to_names_.size();
}


Schema Schema::ProjectByIndices(const std::vector<size_t>& column_indices) const {
    std::vector<std::string> names;
    std::vector<Types> types;
    names.reserve(column_indices.size());
    types.reserve(column_indices.size());

    for (size_t index : column_indices) {
        names.push_back(ColumnName(index));
        types.push_back(ColumnType(index));
    }

    return FromColumns(std::move(names), std::move(types));
}

Schema Schema::ProjectByNames(const std::vector<std::string>& column_names) const {
    std::vector<size_t> indices;
    indices.reserve(column_names.size());

    for (const std::string& name : column_names) {
        indices.push_back(ColumnIndex(name));
    }

    return ProjectByIndices(indices);
}
