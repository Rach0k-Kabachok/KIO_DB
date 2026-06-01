#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "execution/operator.h"
#include "global/columnar_types.h"

class SortOperatorBase {
public:
    enum class SortOrder {
        ASC,
        DESC
    };

    struct SortKey {
        std::string column_name;
        SortOrder order = SortOrder::ASC;
    };

protected:
    explicit SortOperatorBase(const std::vector<SortKey>& sort_keys);
    virtual ~SortOperatorBase() = default;

    struct SortColumn {
        size_t column_idx = 0;
        SortOrder order = SortOrder::ASC;
    };

    std::vector<SortColumn> MakeSortColumns(const Schema& schema) const;

    static bool RowComesBefore(
        const ctp::ColumnarBatch& lhs_columns, size_t lhs_row,
        const ctp::ColumnarBatch& rhs_columns, size_t rhs_row,
        const std::vector<SortColumn>& sort_columns);
    static ctp::ColumnarBatch MakeOutputColumnsByRowIds(
        const ExecBatch& batch, const std::vector<size_t>& row_ids);

    std::vector<SortKey> sort_keys_;
};

using SortOrder = SortOperatorBase::SortOrder;
using SortKey = SortOperatorBase::SortKey;

class SortOperator: public IOperator, protected SortOperatorBase {
public:
    SortOperator(std::unique_ptr<IOperator> child_op,
                 const std::vector<SortKey>& sort_keys);

    std::optional<ExecBatch> Next() override;
    ~SortOperator() override = default;
private:
    std::unique_ptr<IOperator> child_op_;
    bool done_ = false;
};

class TopKOperator: public IOperator, protected SortOperatorBase {
public:
    TopKOperator(std::unique_ptr<IOperator> child_op,
                 const std::vector<SortKey>& sort_keys, size_t limit);

    std::optional<ExecBatch> Next() override;
    ~TopKOperator() override = default;
private:
    std::unique_ptr<IOperator> child_op_;
    size_t limit_;

    bool done_ = false;
};
