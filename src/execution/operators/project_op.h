#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "execution/operator.h"

class ProjectOperator: public IOperator {
public:
    ProjectOperator(std::unique_ptr<IOperator> child_op,
                    std::vector<std::string> column_names);

    std::optional<ExecBatch> Next() override;
    ~ProjectOperator() override = default;
private:
    std::unique_ptr<IOperator> child_op_;
    std::vector<std::string> column_names_;
    std::vector<size_t> column_indices_;
    std::shared_ptr<const Schema> output_schema_;
};
