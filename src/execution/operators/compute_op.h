#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "execution/operator.h"
#include "global/scalar_value.h"
#include "global/schema.h"

class ComputeOperator: public IOperator {
public:
    using RowComputer = std::function<scalar::Value(
        const ExecBatch& batch, size_t row_idx)>;

    struct ComputedColumnSpec {
        std::string name;
        Schema::Types type = Schema::BIGINT;
        RowComputer compute;
    };

    ComputeOperator(std::unique_ptr<IOperator> child_op,
                    std::vector<ComputedColumnSpec> computed_columns);

    std::optional<ExecBatch> Next() override;
    ~ComputeOperator() override = default;
private:
    std::unique_ptr<IOperator> child_op_;
    std::vector<ComputedColumnSpec> computed_columns_;
    std::shared_ptr<const Schema> output_schema_;
};
