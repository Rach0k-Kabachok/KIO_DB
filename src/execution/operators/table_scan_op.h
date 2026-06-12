#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "execution/operator.h"
#include "global/scalar_value.h"
#include "global/schema.h"
#include "transport/kio/kio_db_reader.h"

struct MinMaxConstraint {
    std::string column_name;
    Schema::Types type = Schema::BIGINT;
    std::optional<scalar::Value> lower;
    std::optional<scalar::Value> upper;
    bool lower_inclusive = true;
    bool upper_inclusive = true;
    bool not_equal = false;
    scalar::Value not_equal_value = int64_t{0};
};

class TableScanOperator: public IOperator {
public:
    TableScanOperator(const std::string& db_filename,
                      const std::vector<std::string>& column_names);
    TableScanOperator(const std::string& db_filename,
                      const std::vector<std::string>& column_names,
                      std::shared_ptr<std::vector<MinMaxConstraint>> constraints);

    std::optional<ExecBatch> Next() override;
    ~TableScanOperator() override = default;
private:
    std::shared_ptr<const Schema> output_schema_;
    KioDbReader reader_;
    std::vector<size_t> column_indices_;
    std::shared_ptr<std::vector<MinMaxConstraint>> constraints_;
};
