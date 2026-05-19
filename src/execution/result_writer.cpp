#include "execution/result_writer.h"

#include <optional>
#include <string>
#include <utility>

ResultWriterOperator::ResultWriterOperator(
    std::unique_ptr<IOperator> child_op, const std::string& csv_filename)
        : child_op_(std::move(child_op)),
          csv_exporter_(csv_filename) {
}

std::optional<ExecBatch> ResultWriterOperator::Next() {
    if (done_) {
        return std::nullopt;
    }

    while (std::optional<ExecBatch> batch = child_op_->Next()) {
        csv_exporter_.ExportBatch(*batch->schema, batch->columns,
                                  batch->row_count);
    }

    done_ = true;
    return std::nullopt;
}
