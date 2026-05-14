#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

struct Schema {
    enum Types {
        BIGINT = 0, // 64-битное целое число
        INTEGER = 1, // 32-битное целое число
        SMALLINT = 2, // 16-битное целое число
        TEXT = 3, // строка переменной длины
        VARCHAR = 4, // строка переменной длины с ограничением размера
        CHAR = 5, // строка фиксированной длины
        TIMESTAMP = 6, // дата и время
        DATE = 7 // только дата
    };

    Schema() = default;

    explicit Schema(
        const std::vector<std::vector<std::string>>& column_names_types);
    
    explicit Schema (const std::string& schema_file);

    void ImplSchema(
        const std::vector<std::vector<std::string>>& column_names_types);
    
    void LoadSchema(const std::string& schema_file);    

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
