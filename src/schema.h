#pragma once

#include <stdexcept>
#include <string>
#include <vector>
#include <unordered_map>

struct Schema {
    enum Types {
        INT64 = 0,
        STRING = 1,
    };

    void ImplSchema(const std::vector<std::vector<std::string>>& column_names_types) {
        if (column_names_types.empty()) {
            return;
        }

        // Первая проверка: все ли строки имеют корректный размер
        for (const auto& row : column_names_types) {
            if (row.size() != 2) {
                throw std::invalid_argument("Invalid schema row size (expected 2 columns: [name, type])");
            }
        }

        // Вторая проверка: валидны ли типы, и сохранение данных
        for (const auto& row : column_names_types) {
            const std::string& col_name = row[0];
            const std::string& col_type = row[1];

            // Проверка на дублирование имен колонок
            if (names_to_index_.find(col_name) != names_to_index_.end()) {
                throw std::invalid_argument("Duplicate column name in schema: " + col_name);
            }

            // Определение типа и добавление в индексы
            Types type;
            if (col_type == "int64") {
                type = INT64;
            } else if (col_type == "string") {
                type = STRING;
            } else {
                throw std::invalid_argument("Invalid schema type: " + col_type +
                                          " (expected 'int64' or 'string')");
            }

            // Заполнение индексов
            names_to_index_[col_name] = last_index_;
            index_to_names_.push_back(col_name);
            index_to_types_.push_back(type);

            last_index_++;
        }

        is_empty_ = false;
    }

    // Получить индекс колонки по имени с проверкой границ
    size_t GetIndex(const std::string& name) const {
        auto it = names_to_index_.find(name);
        if (it == names_to_index_.end()) {
            throw std::out_of_range("Column not found in schema: " + name);
        }
        return it->second;
    }

    // Получить тип колонки по индексу с проверкой границ
    Types SearchTypeByIndex(size_t index) const {
        if (index >= index_to_types_.size()) {
            throw std::out_of_range("Column index out of range: " + std::to_string(index));
        }
        return index_to_types_[index];
    }

    // Получить имя колонки по индексу с проверкой границ
    const std::string& SearchNameByIndex(size_t index) const {
        if (index >= index_to_names_.size()) {
            throw std::out_of_range("Column index out of range: " + std::to_string(index));
        }
        return index_to_names_[index];
    }

    // Получить количество колонок в схеме
    size_t GetColumnCount() const {
        return last_index_;
    }

    bool IsEmpty() const {
        return is_empty_;
    }

    const std::unordered_map<std::string, size_t> &GetNameToIndex() {
        return names_to_index_;
    }

    const std::vector<Types> &GetIndexToType() {
        return index_to_types_;
    }

    const std::vector<std::string> &GetIndexToName() {
        return index_to_names_;
    }

private:
    std::unordered_map<std::string, size_t> names_to_index_;
    std::vector<Types> index_to_types_;
    std::vector<std::string> index_to_names_;

    size_t last_index_ = 0;
    bool is_empty_ = true;
};
