#pragma once

#include <string>
#include <vector>
#include <unordered_map>

struct Schema {
    enum Types {
        INT64 = 0,
        STRING = 1,
    };

    Schema() = default;

    explicit Schema(
        const std::vector<std::vector<std::string>>& column_names_types);

    void ImplSchema(
        const std::vector<std::vector<std::string>>& column_names_types);

    const std::unordered_map<std::string, size_t>& GetNameToIndex() const;

    const std::vector<Types>& GetIndexToType() const;

    const std::vector<std::string>& GetIndexToName() const;

    size_t GetIndex(const std::string& name) const;

    Types SearchTypeByIndex(size_t index) const;
    
    const std::string& SearchNameByIndex(size_t index) const;

    const Schema& GetSchema() const;

    Schema GetSchema();

    size_t GetColumnCount() const;

    bool IsEmpty() const;

private:
    std::unordered_map<std::string, size_t> names_to_index_;
    std::vector<Types> index_to_types_;
    std::vector<std::string> index_to_names_;

    size_t schema_size_ = 0;
    bool is_empty_ = true;
};
