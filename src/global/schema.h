#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

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
    
    explicit Schema(const std::string& schema_file);

    static Schema FromRows(
        const std::vector<std::vector<std::string>>& column_names_types);

    static Schema FromColumns(std::vector<std::string> column_names,
                              std::vector<Types> column_types);

    static Types ParseType(const std::string& type);

    static std::string TypeToString(Types type);

    size_t ColumnIndex(const std::string& name) const;

    Types ColumnType(size_t index) const;

    Types ColumnType(const std::string& name) const;

    const std::string& ColumnName(size_t index) const;

    size_t ColumnCount() const;

    Schema ProjectByIndices(const std::vector<size_t>& column_indices) const;

    Schema ProjectByNames(const std::vector<std::string>& column_names) const;

private:
    void ResetFromRows(
        const std::vector<std::vector<std::string>>& column_names_types);

    void ResetFromColumns(std::vector<std::string> column_names,
                          std::vector<Types> column_types);

    void LoadFromCsv(const std::string& schema_file);

    void Clear();

    std::unordered_map<std::string, size_t> names_to_index_;
    std::vector<Types> index_to_types_;
    std::vector<std::string> index_to_names_;
};
