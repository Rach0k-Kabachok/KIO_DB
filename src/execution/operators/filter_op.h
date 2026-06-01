#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>

#include "execution/operator.h"

class FilterOperator: public IOperator {
public:
    using RowPredicate = std::function<bool(const ExecBatch& batch,
                                            size_t row_idx)>;

    FilterOperator(std::unique_ptr<IOperator> child_op,
                   RowPredicate predicate);

    std::optional<ExecBatch> Next() override;
    ~FilterOperator() override = default;
private:
    std::unique_ptr<IOperator> child_op_;
    RowPredicate predicate_;
};
