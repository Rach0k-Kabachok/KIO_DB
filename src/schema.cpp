#include "schema.h"

#include <stdexcept>
#include <string>
#include <vector>
#include <unordered_map>


Schema::Schema(
        const std::vector<std::vector<std::string>>& column_names_types) {
    ImplSchema(column_names_types);
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
    }

    throw std::invalid_argument("Invalid schema type: " + type);
}

Schema::Types Schema::TypeFromId(uint8_t type_id) {
    switch (type_id) {
    case BIGINT:
        return BIGINT;
    case INTEGER:
        return INTEGER;
    case SMALLINT:
        return SMALLINT;
    case TEXT:
        return TEXT;
    case VARCHAR:
        return VARCHAR;
    case CHAR:
        return CHAR;
    case TIMESTAMP:
        return TIMESTAMP;
    case DATE:
        return DATE;
    }

    throw std::invalid_argument("Invalid schema type id: " +
                                std::to_string(type_id));
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
    }

    throw std::invalid_argument("Invalid schema type");
}

void Schema::ImplSchema(
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

void Schema::Clear() {
    names_to_index_.clear();
    index_to_types_.clear();
    index_to_names_.clear();
}


const std::unordered_map<std::string, size_t>& Schema::GetNameToIndex() const {
    return names_to_index_;
}


const std::vector<Schema::Types>& Schema::GetIndexToType() const {
    return index_to_types_;
}


const std::vector<std::string>& Schema::GetIndexToName() const {
    return index_to_names_;
}


size_t Schema::GetIndex(const std::string& name) const {
    auto it = names_to_index_.find(name);
    if (it == names_to_index_.end()) {
        throw std::out_of_range("Column not found in schema: " + name);
    }
    return it->second;
}


Schema::Types Schema::SearchTypeByIndex(size_t index) const {
    if (index >= index_to_types_.size()) {
        throw std::out_of_range("Column index out of range");
    }
    return index_to_types_[index];
}


const std::string& Schema::SearchNameByIndex(size_t index) const {
    if (index >= index_to_names_.size()) {
        throw std::out_of_range("Column index out of range: " +
                                std::to_string(index));
    }
    return index_to_names_[index];
}


size_t Schema::GetColumnCount() const {
    return index_to_names_.size();
}


bool Schema::IsEmpty() const {
    return index_to_names_.empty();
}
