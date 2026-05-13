#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

struct Schema {
    enum Types {
        BIGINT = 0, // 64 bit integer
        INTEGER = 1, // 32 bit integer
        SMALLINT = 2, // 16 bit integer
        TEXT = 3, // variable-length string
        VARCHAR = 4, // variable-length string with maximum length
        CHAR = 5, // fixed-length string
        TIMESTAMP = 6, // date and time
        DATE = 7 // date only
    };

    Schema() = default;

    explicit Schema(
        const std::vector<std::vector<std::string>>& column_names_types);

    void ImplSchema(
        const std::vector<std::vector<std::string>>& column_names_types);

    void Clear();

    static Types ParseType(const std::string& type);

    static Types TypeFromId(uint8_t type_id);

    static std::string TypeToString(Types type);

    const std::unordered_map<std::string, size_t>& GetNameToIndex() const;

    const std::vector<Types>& GetIndexToType() const;

    const std::vector<std::string>& GetIndexToName() const;

    size_t GetIndex(const std::string& name) const;

    Types SearchTypeByIndex(size_t index) const;
    
    const std::string& SearchNameByIndex(size_t index) const;

    size_t GetColumnCount() const;

    bool IsEmpty() const;

private:
    std::unordered_map<std::string, size_t> names_to_index_;
    std::vector<Types> index_to_types_;
    std::vector<std::string> index_to_names_;
};
