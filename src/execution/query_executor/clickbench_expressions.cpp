#include "execution/query_executor/clickbench_expressions.h"

#include <cstdint>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "global/column_operations.h"
#include "global/scalar_value.h"
#include "global/schema.h"
#include "transport/csv/csv_type_parser.h"

namespace clickbench {
namespace {

template <typename T>
const std::vector<T>& Values(const ExecBatch& batch, size_t column_idx) {
    return ctp::GetColumnData<T>(batch.columns[column_idx]);
}

int64_t MinuteOfTimestamp(int64_t timestamp) {
    return timestamp / csv::kSecondsPerMinute % csv::kMinutesPerHour;
}

int64_t TruncateToMinute(int64_t timestamp) {
    return timestamp - timestamp % csv::kSecondsPerMinute;
}

std::string RefererDomain(const std::string& referer) {
    size_t pos = 0;
    if (referer.starts_with("http://")) {
        pos = 7;
    } else if (referer.starts_with("https://")) {
        pos = 8;
    } else {
        return referer;
    }

    if (referer.compare(pos, 4, "www.") == 0) {
        pos += 4;
    }

    const size_t slash = referer.find('/', pos);
    if (slash == std::string::npos) {
        return referer;
    }
    return referer.substr(pos, slash - pos);
}

}  // namespace

int32_t Date(std::string_view value) {
    return csv::DateToDays(value);
}

ComputeOperator::ComputedColumnSpec StringLength(std::string column,
                                                 std::string result) {
    return ComputeOperator::ComputedColumnSpec{
        std::move(result), Schema::BIGINT,
        [column = std::move(column), column_idx = std::optional<size_t>{}](
            const ExecBatch& batch, size_t row_idx) mutable -> scalar::Value {
            if (!column_idx.has_value()) {
                column_idx = batch.schema->ColumnIndex(column);
            }
            return static_cast<int64_t>(
                Values<std::string>(batch, *column_idx)[row_idx].size());
        }};
}

ComputeOperator::ComputedColumnSpec Int32Minus(std::string column,
                                               int32_t value,
                                               std::string result) {
    return ComputeOperator::ComputedColumnSpec{
        std::move(result), Schema::INTEGER,
        [column = std::move(column), value,
         column_idx = std::optional<size_t>{}](
            const ExecBatch& batch, size_t row_idx) mutable -> scalar::Value {
            if (!column_idx.has_value()) {
                column_idx = batch.schema->ColumnIndex(column);
            }
            return Values<int32_t>(batch, *column_idx)[row_idx] - value;
        }};
}

ComputeOperator::ComputedColumnSpec SmallIntPlus(std::string column,
                                                 int64_t value,
                                                 std::string result) {
    return ComputeOperator::ComputedColumnSpec{
        std::move(result), Schema::BIGINT,
        [column = std::move(column), value,
         column_idx = std::optional<size_t>{}](
            const ExecBatch& batch, size_t row_idx) mutable -> scalar::Value {
            if (!column_idx.has_value()) {
                column_idx = batch.schema->ColumnIndex(column);
            }
            return static_cast<int64_t>(
                Values<int16_t>(batch, *column_idx)[row_idx]) + value;
        }};
}

ComputeOperator::ComputedColumnSpec MinuteColumn(std::string column,
                                                 std::string result) {
    return ComputeOperator::ComputedColumnSpec{
        std::move(result), Schema::BIGINT,
        [column = std::move(column), column_idx = std::optional<size_t>{}](
            const ExecBatch& batch, size_t row_idx) mutable -> scalar::Value {
            if (!column_idx.has_value()) {
                column_idx = batch.schema->ColumnIndex(column);
            }
            return MinuteOfTimestamp(
                Values<int64_t>(batch, *column_idx)[row_idx]);
        }};
}

ComputeOperator::ComputedColumnSpec DateTruncMinute(std::string column,
                                                    std::string result) {
    return ComputeOperator::ComputedColumnSpec{
        std::move(result), Schema::TIMESTAMP,
        [column = std::move(column), column_idx = std::optional<size_t>{}](
            const ExecBatch& batch, size_t row_idx) mutable -> scalar::Value {
            if (!column_idx.has_value()) {
                column_idx = batch.schema->ColumnIndex(column);
            }
            return TruncateToMinute(
                Values<int64_t>(batch, *column_idx)[row_idx]);
        }};
}

ComputeOperator::ComputedColumnSpec DomainColumn(std::string column,
                                                 std::string result) {
    return ComputeOperator::ComputedColumnSpec{
        std::move(result), Schema::TEXT,
        [column = std::move(column), column_idx = std::optional<size_t>{}](
            const ExecBatch& batch, size_t row_idx) mutable -> scalar::Value {
            if (!column_idx.has_value()) {
                column_idx = batch.schema->ColumnIndex(column);
            }
            return RefererDomain(
                Values<std::string>(batch, *column_idx)[row_idx]);
        }};
}

ComputeOperator::ComputedColumnSpec ConstantOne() {
    return ComputeOperator::ComputedColumnSpec{
        "one", Schema::BIGINT,
        [](const ExecBatch&, size_t) -> scalar::Value { return int64_t{1}; }};
}

ComputeOperator::ComputedColumnSpec AggregatedSumPlus(std::string sum_column,
                                                      std::string count_column,
                                                      int64_t multiplier,
                                                      std::string result) {
    return ComputeOperator::ComputedColumnSpec{
        std::move(result), Schema::BIGINT,
        [sum_column = std::move(sum_column),
         count_column = std::move(count_column),
         multiplier,
         sum_idx = std::optional<size_t>{},
         count_idx = std::optional<size_t>{}](
            const ExecBatch& batch, size_t row_idx) mutable -> scalar::Value {
            if (!sum_idx.has_value()) {
                sum_idx = batch.schema->ColumnIndex(sum_column);
                count_idx = batch.schema->ColumnIndex(count_column);
            }
            return Values<int64_t>(batch, *sum_idx)[row_idx] +
                   multiplier * Values<int64_t>(batch, *count_idx)[row_idx];
        }};
}

ComputeOperator::ComputedColumnSpec CaseSource() {
    return ComputeOperator::ComputedColumnSpec{
        "Src", Schema::TEXT,
        [search_idx = std::optional<size_t>{},
         adv_idx = std::optional<size_t>{},
         referer_idx = std::optional<size_t>{}](
            const ExecBatch& batch, size_t row_idx) mutable -> scalar::Value {
            if (!search_idx.has_value()) {
                search_idx = batch.schema->ColumnIndex("SearchEngineID");
                adv_idx = batch.schema->ColumnIndex("AdvEngineID");
                referer_idx = batch.schema->ColumnIndex("Referer");
            }
            if (Values<int16_t>(batch, *search_idx)[row_idx] == 0 &&
                Values<int16_t>(batch, *adv_idx)[row_idx] == 0) {
                return Values<std::string>(batch, *referer_idx)[row_idx];
            }
            return std::string{};
        }};
}

ComputeOperator::ComputedColumnSpec UrlDestination() {
    return ComputeOperator::ComputedColumnSpec{
        "Dst", Schema::TEXT,
        [url_idx = std::optional<size_t>{}](
            const ExecBatch& batch, size_t row_idx) mutable -> scalar::Value {
            if (!url_idx.has_value()) {
                url_idx = batch.schema->ColumnIndex("URL");
            }
            return Values<std::string>(batch, *url_idx)[row_idx];
        }};
}

}  // namespace clickbench
