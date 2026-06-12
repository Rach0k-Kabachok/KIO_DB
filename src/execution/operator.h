#pragma once

#include <cstddef>
#include <memory>
#include <optional>

#include "global/columnar_types.h"
#include "global/schema.h"

struct ExecBatch {
    ctp::ColumnarBatch columns;
    std::shared_ptr<const Schema> schema;
    size_t row_count = 0;
};

class IOperator {
public:
    IOperator() = default;
    virtual std::optional<ExecBatch> Next() = 0;
    virtual ~IOperator() = default;
};
