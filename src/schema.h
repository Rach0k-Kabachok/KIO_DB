#pragma once

#include <string>
#include <vector>
#include <unordered_map>

struct Schema {
    enum Types {
        INT64,
        STRING,
    };

    void ImplSchema(const std::vector<std::vector<std::string> > &column_names_types) {
        if (column_names_types.empty() || column_names_types[0].size() != 2) {
            throw std::invalid_argument("Invalid schema size");
        }

        for (auto &row: column_names_types) {
            names_to_index_[row[0]] = last_index_;
            index_to_names_[last_index_] = row[0];

            if (row[1] == "int64") {
                index_to_types_[last_index_] = INT64;
            } else if (row[1] == "string") {
                index_to_types_[last_index_] = STRING;
            } else {
                throw std::invalid_argument("Invalid schema type");
            }

            last_index_++;
        }
    }

    size_t GetIndex(const std::string& name) const {
        return names_to_index_.find(name)->second;
    }

    const Types &SearchTypeByIndex(size_t index) const {
        return index_to_types_[index];
    }

    const std::string &SearchNameByIndex(size_t index) const {
        return index_to_names_[index];
    }

private:

    std::unordered_map<std::string, size_t> names_to_index_;
    std::vector<Types> index_to_types_;
    std::vector<std::string> index_to_names_;

    size_t last_index_ = 0;
};
