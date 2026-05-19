#pragma once

#include <memory>
#include <optional>
#include <string>

#include "execution/operators.h"
#include "transport/csv/csv_exporter.h"

class ResultWriterOperator : public IOperator {
public:
    ResultWriterOperator(std::unique_ptr<IOperator> child_op,
                         const std::string& csv_filename);

    std::optional<ExecBatch> Next() override;

private:
    std::unique_ptr<IOperator> child_op_;
    CsvExporter csv_exporter_;
    bool done_ = false;
};
