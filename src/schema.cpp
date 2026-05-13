#include "schema.h"

#include <stdexcept>
#include <string>
#include <vector>
#include <unordered_map>


Schema::Schema(
        const std::vector<std::vector<std::string>>& column_names_types) {
    ImplSchema(column_names_types);
}


void Schema::ImplSchema(
        const std::vector<std::vector<std::string>>& column_names_types) {
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

        Types type;
        if (col_type == "int64") {
            type = INT64;
        } else if (col_type == "string") {
            type = STRING;
        } else {
            throw std::invalid_argument("Invalid schema type: " + col_type);
        }

        names_to_index_[col_name] = schema_size_;
        index_to_names_.push_back(col_name);
        index_to_types_.push_back(type);

        schema_size_++;
    }

    is_empty_ = false;
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
    if (index >= schema_size_) {
        throw std::out_of_range("Column index out of range");
    }
    return index_to_types_[index];
}


const std::string& Schema::SearchNameByIndex(size_t index) const {
    if (index >= schema_size_) {
        throw std::out_of_range("Column index out of range: " +
                                std::to_string(index));
    }
    return index_to_names_[index];
}


const Schema& Schema::GetSchema() const {
    return *this;
}


Schema Schema::GetSchema() {
    return *this;
}


size_t Schema::GetColumnCount() const {
    return schema_size_;
}


bool Schema::IsEmpty() const {
    return is_empty_;
}
