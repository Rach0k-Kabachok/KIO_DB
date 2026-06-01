#pragma once

#include <cstddef>
#include <memory>
#include <optional>

#include "execution/operator.h"

class LimitOperator: public IOperator {
public:
    LimitOperator(std::unique_ptr<IOperator> child_op, size_t limit,
                  size_t offset = 0);

    std::optional<ExecBatch> Next() override;
    ~LimitOperator() override = default;
private:
    std::unique_ptr<IOperator> child_op_;
    size_t limit_;
    size_t offset_ = 0;
    size_t skipped_ = 0;
    size_t returned_ = 0;
};
