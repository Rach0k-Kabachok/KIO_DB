#include "execution/clickbench_queries.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "execution/operators.h"
#include "global/column_operations.h"
#include "global/columnar_types.h"
#include "global/schema.h"
#include "transport/csv/csv_type_parser.h"
#include "transport/kio/kio_db_reader.h"

namespace {

using OperatorPtr = std::unique_ptr<IOperator>;
using Predicate = FilterOperator::RowPredicate;
using ConstraintList = std::shared_ptr<std::vector<MinMaxConstraint>>;

std::vector<std::string> AllColumns(const std::string& db_filename) {
    KioDbReader reader(db_filename);
    const Schema& schema = reader.GetSchema();
    std::vector<std::string> columns;
    columns.reserve(schema.ColumnCount());
    for (size_t idx = 0; idx < schema.ColumnCount(); idx++) {
        columns.push_back(schema.ColumnName(idx));
    }
    return columns;
}

OperatorPtr Scan(const std::string& db_filename,
                 const std::vector<std::string>& columns,
                 ConstraintList constraints = nullptr) {
    return std::make_unique<TableScanOperator>(
        db_filename, columns, std::move(constraints));
}

ExecBatch EmptyBatch() {
    return ExecBatch{ctp::ColumnarBatch{},
                     std::make_shared<Schema>(
                         Schema::FromColumns({}, {})),
                     0};
}

ExecBatch Collect(OperatorPtr op) {
    std::optional<ExecBatch> optional_batch = op->Next();
    if (!optional_batch.has_value()) {
        return EmptyBatch();
    }

    ExecBatch result = std::move(*optional_batch);
    while ((optional_batch = op->Next()).has_value()) {
        ExecBatch& batch = *optional_batch;
        for (size_t row_idx = 0; row_idx < batch.row_count; row_idx++) {
            for (size_t col_idx = 0; col_idx < batch.columns.size(); col_idx++) {
                ctp::AppendColumnValue(result.columns[col_idx],
                                       batch.columns[col_idx],
                                       row_idx,
                                       result.schema->ColumnType(col_idx));
            }
            result.row_count++;
        }
    }

    return result;
}

ExecBatch SingleInt64(const std::string& name, int64_t value) {
    return ExecBatch{
        ctp::ColumnarBatch{ctp::Column{std::vector<int64_t>{value}}},
        std::make_shared<Schema>(
            Schema::FromColumns({name}, {Schema::BIGINT})),
        1};
}

template <typename T>
const std::vector<T>& Values(const ExecBatch& batch, size_t column_idx) {
    return ctp::GetColumnData<T>(batch.columns[column_idx]);
}

template <typename T>
Predicate EqualTo(std::string column_name, T value) {
    return [column_name = std::move(column_name), value,
            column_idx = std::optional<size_t>{}](
               const ExecBatch& batch, size_t row_idx) mutable {
        if (!column_idx.has_value()) {
            column_idx = batch.schema->ColumnIndex(column_name);
        }
        return Values<T>(batch, *column_idx)[row_idx] == value;
    };
}

template <typename T>
Predicate NotEqualTo(std::string column_name, T value) {
    return [column_name = std::move(column_name), value,
            column_idx = std::optional<size_t>{}](
               const ExecBatch& batch, size_t row_idx) mutable {
        if (!column_idx.has_value()) {
            column_idx = batch.schema->ColumnIndex(column_name);
        }
        return Values<T>(batch, *column_idx)[row_idx] != value;
    };
}

template <typename T>
Predicate GreaterEqual(std::string column_name, T value) {
    return [column_name = std::move(column_name), value,
            column_idx = std::optional<size_t>{}](
               const ExecBatch& batch, size_t row_idx) mutable {
        if (!column_idx.has_value()) {
            column_idx = batch.schema->ColumnIndex(column_name);
        }
        return Values<T>(batch, *column_idx)[row_idx] >= value;
    };
}

template <typename T>
Predicate LessEqual(std::string column_name, T value) {
    return [column_name = std::move(column_name), value,
            column_idx = std::optional<size_t>{}](
               const ExecBatch& batch, size_t row_idx) mutable {
        if (!column_idx.has_value()) {
            column_idx = batch.schema->ColumnIndex(column_name);
        }
        return Values<T>(batch, *column_idx)[row_idx] <= value;
    };
}

Predicate StringNotEmpty(std::string column_name) {
    return [column_name = std::move(column_name),
            column_idx = std::optional<size_t>{}](
               const ExecBatch& batch, size_t row_idx) mutable {
        if (!column_idx.has_value()) {
            column_idx = batch.schema->ColumnIndex(column_name);
        }
        return !Values<std::string>(batch, *column_idx)[row_idx].empty();
    };
}

Predicate Contains(std::string column_name, std::string needle) {
    return [column_name = std::move(column_name), needle = std::move(needle),
            column_idx = std::optional<size_t>{}](
               const ExecBatch& batch, size_t row_idx) mutable {
        if (!column_idx.has_value()) {
            column_idx = batch.schema->ColumnIndex(column_name);
        }
        const std::string& value =
            Values<std::string>(batch, *column_idx)[row_idx];
        return value.find(needle) != std::string::npos;
    };
}

Predicate NotContains(std::string column_name, std::string needle) {
    return [predicate = Contains(std::move(column_name), std::move(needle))](
               const ExecBatch& batch, size_t row_idx) mutable {
        return !predicate(batch, row_idx);
    };
}

Predicate InSmallInt(std::string column_name, int16_t lhs, int16_t rhs) {
    return [column_name = std::move(column_name), lhs, rhs,
            column_idx = std::optional<size_t>{}](
               const ExecBatch& batch, size_t row_idx) mutable {
        if (!column_idx.has_value()) {
            column_idx = batch.schema->ColumnIndex(column_name);
        }
        const int16_t value = Values<int16_t>(batch, *column_idx)[row_idx];
        return value == lhs || value == rhs;
    };
}

Predicate And(std::vector<Predicate> predicates) {
    return [predicates = std::move(predicates)](
               const ExecBatch& batch, size_t row_idx) mutable {
        for (Predicate& predicate : predicates) {
            if (!predicate(batch, row_idx)) {
                return false;
            }
        }
        return true;
    };
}

OperatorPtr Filter(OperatorPtr child, Predicate predicate) {
    return std::make_unique<FilterOperator>(
        std::move(child), std::move(predicate));
}

OperatorPtr Project(OperatorPtr child, std::vector<std::string> columns) {
    return std::make_unique<ProjectOperator>(std::move(child),
                                             std::move(columns));
}

OperatorPtr Compute(OperatorPtr child,
                    std::vector<ComputeOperator::ComputedColumnSpec> specs) {
    return std::make_unique<ComputeOperator>(std::move(child), std::move(specs));
}

OperatorPtr Global(OperatorPtr child, std::vector<AggregateSpec> aggregates) {
    return std::make_unique<GlobalAgrOperator>(std::move(child),
                                               std::move(aggregates));
}

OperatorPtr Group(OperatorPtr child, std::vector<std::string> keys,
                  std::vector<AggregateSpec> aggregates) {
    return std::make_unique<GroupAgrOperator>(
        std::move(child), std::move(keys), std::move(aggregates));
}

OperatorPtr Sort(OperatorPtr child, std::vector<SortKey> keys) {
    return std::make_unique<SortOperator>(std::move(child), std::move(keys));
}

OperatorPtr TopK(OperatorPtr child, std::vector<SortKey> keys, size_t limit) {
    return std::make_unique<TopKOperator>(
        std::move(child), std::move(keys), limit);
}

OperatorPtr Limit(OperatorPtr child, size_t limit, size_t offset = 0) {
    return std::make_unique<LimitOperator>(std::move(child), limit, offset);
}

OperatorPtr OrderedLimit(OperatorPtr child, std::vector<SortKey> keys,
                         size_t limit, size_t offset = 0) {
    if (offset == 0) {
        return TopK(std::move(child), std::move(keys), limit);
    }
    return Limit(TopK(std::move(child), std::move(keys), limit + offset),
                 limit, offset);
}

AggregateSpec Count(std::string name = "count") {
    return AggregateSpec{AggregateKind::COUNT, "", std::move(name)};
}

AggregateSpec Sum(std::string column, std::string name) {
    return AggregateSpec{AggregateKind::SUM, std::move(column), std::move(name)};
}

AggregateSpec Avg(std::string column, std::string name) {
    return AggregateSpec{AggregateKind::AVG, std::move(column), std::move(name)};
}

AggregateSpec Min(std::string column, std::string name) {
    return AggregateSpec{AggregateKind::MIN, std::move(column), std::move(name)};
}

AggregateSpec Max(std::string column, std::string name) {
    return AggregateSpec{AggregateKind::MAX, std::move(column), std::move(name)};
}

AggregateSpec CountDistinct(std::string column, std::string name) {
    return AggregateSpec{
        AggregateKind::COUNT_DISTINCT, std::move(column), std::move(name)};
}

MinMaxConstraint EqConstraint(std::string column, Schema::Types type,
                              VarType value) {
    return MinMaxConstraint{
        std::move(column), type, value, value, true, true, false, int64_t{0}};
}

MinMaxConstraint BetweenConstraint(std::string column, Schema::Types type,
                                   VarType lower, VarType upper) {
    return MinMaxConstraint{
        std::move(column), type, lower, upper, true, true, false, int64_t{0}};
}

MinMaxConstraint NotEqConstraint(std::string column, Schema::Types type,
                                 VarType value) {
    return MinMaxConstraint{
        std::move(column), type, std::nullopt, std::nullopt,
        true, true, true, value};
}

ConstraintList Constraints(std::vector<MinMaxConstraint> constraints) {
    return std::make_shared<std::vector<MinMaxConstraint>>(
        std::move(constraints));
}

int32_t Date(std::string_view value) {
    return csv::DateToDays(value);
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

ComputeOperator::ComputedColumnSpec StringLength(std::string column,
                                                 std::string result) {
    return ComputeOperator::ComputedColumnSpec{
        std::move(result), Schema::BIGINT,
        [column = std::move(column), column_idx = std::optional<size_t>{}](
            const ExecBatch& batch, size_t row_idx) mutable -> VarType {
            if (!column_idx.has_value()) {
                column_idx = batch.schema->ColumnIndex(column);
            }
            return int64_t(Values<std::string>(batch, *column_idx)[row_idx].size());
        }};
}

ComputeOperator::ComputedColumnSpec Int32Minus(std::string column,
                                               int32_t value,
                                               std::string result) {
    return ComputeOperator::ComputedColumnSpec{
        std::move(result), Schema::INTEGER,
        [column = std::move(column), value,
         column_idx = std::optional<size_t>{}](
            const ExecBatch& batch, size_t row_idx) mutable -> VarType {
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
            const ExecBatch& batch, size_t row_idx) mutable -> VarType {
            if (!column_idx.has_value()) {
                column_idx = batch.schema->ColumnIndex(column);
            }
            return int64_t(Values<int16_t>(batch, *column_idx)[row_idx]) + value;
        }};
}

ComputeOperator::ComputedColumnSpec MinuteColumn(std::string column,
                                                 std::string result) {
    return ComputeOperator::ComputedColumnSpec{
        std::move(result), Schema::BIGINT,
        [column = std::move(column), column_idx = std::optional<size_t>{}](
            const ExecBatch& batch, size_t row_idx) mutable -> VarType {
            if (!column_idx.has_value()) {
                column_idx = batch.schema->ColumnIndex(column);
            }
            return MinuteOfTimestamp(Values<int64_t>(batch, *column_idx)[row_idx]);
        }};
}

ComputeOperator::ComputedColumnSpec DateTruncMinute(std::string column,
                                                    std::string result) {
    return ComputeOperator::ComputedColumnSpec{
        std::move(result), Schema::TIMESTAMP,
        [column = std::move(column), column_idx = std::optional<size_t>{}](
            const ExecBatch& batch, size_t row_idx) mutable -> VarType {
            if (!column_idx.has_value()) {
                column_idx = batch.schema->ColumnIndex(column);
            }
            return TruncateToMinute(Values<int64_t>(batch, *column_idx)[row_idx]);
        }};
}

ComputeOperator::ComputedColumnSpec DomainColumn(std::string column,
                                                 std::string result) {
    return ComputeOperator::ComputedColumnSpec{
        std::move(result), Schema::TEXT,
        [column = std::move(column), column_idx = std::optional<size_t>{}](
            const ExecBatch& batch, size_t row_idx) mutable -> VarType {
            if (!column_idx.has_value()) {
                column_idx = batch.schema->ColumnIndex(column);
            }
            return RefererDomain(Values<std::string>(batch, *column_idx)[row_idx]);
        }};
}

ComputeOperator::ComputedColumnSpec ConstantOne() {
    return ComputeOperator::ComputedColumnSpec{
        "one", Schema::BIGINT,
        [](const ExecBatch&, size_t) -> VarType { return int64_t{1}; }};
}

ComputeOperator::ComputedColumnSpec CaseSource() {
    return ComputeOperator::ComputedColumnSpec{
        "Src", Schema::TEXT,
        [search_idx = std::optional<size_t>{},
         adv_idx = std::optional<size_t>{},
         referer_idx = std::optional<size_t>{}](
            const ExecBatch& batch, size_t row_idx) mutable -> VarType {
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
            const ExecBatch& batch, size_t row_idx) mutable -> VarType {
            if (!url_idx.has_value()) {
                url_idx = batch.schema->ColumnIndex("URL");
            }
            return Values<std::string>(batch, *url_idx)[row_idx];
        }};
}

Predicate JulyFilters(std::vector<Predicate> extra = {}) {
    std::vector<Predicate> predicates{
        EqualTo<int32_t>("CounterID", 62),
        GreaterEqual<int32_t>("EventDate", Date("2013-07-01")),
        LessEqual<int32_t>("EventDate", Date("2013-07-31")),
        EqualTo<int16_t>("IsRefresh", 0)};
    predicates.insert(predicates.end(),
                      std::make_move_iterator(extra.begin()),
                      std::make_move_iterator(extra.end()));
    return And(std::move(predicates));
}

ConstraintList JulyConstraints(std::vector<MinMaxConstraint> extra = {}) {
    std::vector<MinMaxConstraint> constraints{
        EqConstraint("CounterID", Schema::INTEGER, int32_t{62}),
        BetweenConstraint("EventDate", Schema::DATE,
                          Date("2013-07-01"), Date("2013-07-31")),
        EqConstraint("IsRefresh", Schema::SMALLINT, int16_t{0})};
    constraints.insert(constraints.end(),
                       std::make_move_iterator(extra.begin()),
                       std::make_move_iterator(extra.end()));
    return Constraints(std::move(constraints));
}

OperatorPtr QueryWithSearchPhrase(std::string db_filename,
                                  std::vector<std::string> columns) {
    return Filter(Scan(db_filename, columns), StringNotEmpty("SearchPhrase"));
}

}  // namespace

ExecBatch ExecuteClickBenchQuery(const std::string& db_filename, int query_id) {
    switch (query_id) {
    case 1: {
        KioDbReader reader(db_filename);
        return SingleInt64("count", reader.GetMetadata().row_count);
    }
    case 2:
        return Collect(Global(
            Filter(Scan(db_filename, {"AdvEngineID"},
                        Constraints({NotEqConstraint(
                            "AdvEngineID", Schema::SMALLINT, int16_t{0})})),
                   NotEqualTo<int16_t>("AdvEngineID", 0)),
            {Count()}));
    case 3:
        return Collect(Global(
            Scan(db_filename, {"AdvEngineID", "ResolutionWidth"}),
            {Sum("AdvEngineID", "sum"),
             Count(),
             Avg("ResolutionWidth", "avg")}));
    case 4:
        return Collect(Global(Scan(db_filename, {"UserID"}),
                              {Avg("UserID", "avg")}));
    case 5:
        return Collect(Global(Scan(db_filename, {"UserID"}),
                              {CountDistinct("UserID", "uniq")}));
    case 6:
        return Collect(Global(Scan(db_filename, {"SearchPhrase"}),
                              {CountDistinct("SearchPhrase", "uniq")}));
    case 7:
        return Collect(Global(Scan(db_filename, {"EventDate"}),
                              {Min("EventDate", "min"),
                               Max("EventDate", "max")}));
    case 8:
        return Collect(Sort(
            Group(Filter(Scan(db_filename, {"AdvEngineID"},
                              Constraints({NotEqConstraint(
                                  "AdvEngineID", Schema::SMALLINT,
                                  int16_t{0})})),
                         NotEqualTo<int16_t>("AdvEngineID", 0)),
                  {"AdvEngineID"}, {Count("c")}),
            {SortKey{"c", SortOrder::DESC}}));
    case 9:
        return Collect(OrderedLimit(
            Group(Scan(db_filename, {"RegionID", "UserID"}),
                  {"RegionID"}, {CountDistinct("UserID", "u")}),
            {SortKey{"u", SortOrder::DESC}}, 10));
    case 10:
        return Collect(OrderedLimit(
            Group(Scan(db_filename,
                       {"RegionID", "AdvEngineID", "ResolutionWidth", "UserID"}),
                  {"RegionID"},
                  {Sum("AdvEngineID", "sum"),
                   Count("c"),
                   Avg("ResolutionWidth", "avg"),
                   CountDistinct("UserID", "uniq")}),
            {SortKey{"c", SortOrder::DESC}}, 10));
    case 11:
        return Collect(OrderedLimit(
            Group(Filter(Scan(db_filename, {"MobilePhoneModel", "UserID"}),
                         StringNotEmpty("MobilePhoneModel")),
                  {"MobilePhoneModel"},
                  {CountDistinct("UserID", "u")}),
            {SortKey{"u", SortOrder::DESC}}, 10));
    case 12:
        return Collect(OrderedLimit(
            Group(Filter(Scan(db_filename,
                              {"MobilePhone", "MobilePhoneModel", "UserID"}),
                         StringNotEmpty("MobilePhoneModel")),
                  {"MobilePhone", "MobilePhoneModel"},
                  {CountDistinct("UserID", "u")}),
            {SortKey{"u", SortOrder::DESC}}, 10));
    case 13:
        return Collect(OrderedLimit(
            Group(Filter(Scan(db_filename, {"SearchPhrase"}),
                         StringNotEmpty("SearchPhrase")),
                  {"SearchPhrase"}, {Count("c")}),
            {SortKey{"c", SortOrder::DESC}}, 10));
    case 14:
        return Collect(OrderedLimit(
            Group(Filter(Scan(db_filename, {"SearchPhrase", "UserID"}),
                         StringNotEmpty("SearchPhrase")),
                  {"SearchPhrase"}, {CountDistinct("UserID", "u")}),
            {SortKey{"u", SortOrder::DESC}}, 10));
    case 15:
        return Collect(OrderedLimit(
            Group(Filter(Scan(db_filename,
                              {"SearchEngineID", "SearchPhrase"}),
                         StringNotEmpty("SearchPhrase")),
                  {"SearchEngineID", "SearchPhrase"}, {Count("c")}),
            {SortKey{"c", SortOrder::DESC}}, 10));
    case 16:
        return Collect(OrderedLimit(
            Group(Scan(db_filename, {"UserID"}), {"UserID"}, {Count("c")}),
            {SortKey{"c", SortOrder::DESC}}, 10));
    case 17:
        return Collect(OrderedLimit(
            Group(Scan(db_filename, {"UserID", "SearchPhrase"}),
                  {"UserID", "SearchPhrase"}, {Count("c")}),
            {SortKey{"c", SortOrder::DESC}}, 10));
    case 18:
        return Collect(Limit(
            Group(Scan(db_filename, {"UserID", "SearchPhrase"}),
                  {"UserID", "SearchPhrase"}, {Count("c")}),
            10));
    case 19:
        return Collect(OrderedLimit(
            Group(Compute(Scan(db_filename,
                               {"UserID", "EventTime", "SearchPhrase"}),
                          {MinuteColumn("EventTime", "m")}),
                  {"UserID", "m", "SearchPhrase"}, {Count("c")}),
            {SortKey{"c", SortOrder::DESC}}, 10));
    case 20:
        return Collect(Project(
            Filter(Scan(db_filename, {"UserID"},
                        Constraints({EqConstraint(
                            "UserID", Schema::BIGINT,
                            int64_t{435090932899640449})})),
                   EqualTo<int64_t>("UserID", 435090932899640449)),
            {"UserID"}));
    case 21:
        return Collect(Global(
            Filter(Scan(db_filename, {"URL"}), Contains("URL", "google")),
            {Count()}));
    case 22:
        return Collect(OrderedLimit(
            Group(Filter(Scan(db_filename, {"SearchPhrase", "URL"}),
                         And({Contains("URL", "google"),
                              StringNotEmpty("SearchPhrase")})),
                  {"SearchPhrase"}, {Min("URL", "min_url"), Count("c")}),
            {SortKey{"c", SortOrder::DESC}}, 10));
    case 23:
        return Collect(OrderedLimit(
            Group(Filter(Scan(db_filename,
                              {"SearchPhrase", "URL", "Title", "UserID"}),
                         And({Contains("Title", "Google"),
                              NotContains("URL", ".google."),
                              StringNotEmpty("SearchPhrase")})),
                  {"SearchPhrase"},
                  {Min("URL", "min_url"),
                   Min("Title", "min_title"),
                   Count("c"),
                   CountDistinct("UserID", "uniq")}),
            {SortKey{"c", SortOrder::DESC}}, 10));
    case 24:
        return Collect(Limit(
            Sort(Filter(Scan(db_filename, AllColumns(db_filename)),
                        Contains("URL", "google")),
                 {SortKey{"EventTime", SortOrder::ASC}}),
            10));
    case 25:
        return Collect(Project(
            Limit(Sort(Filter(Scan(db_filename, {"SearchPhrase", "EventTime"}),
                              StringNotEmpty("SearchPhrase")),
                       {SortKey{"EventTime", SortOrder::ASC}}),
                  10),
            {"SearchPhrase"}));
    case 26:
        return Collect(Limit(
            Sort(Filter(Scan(db_filename, {"SearchPhrase"}),
                        StringNotEmpty("SearchPhrase")),
                 {SortKey{"SearchPhrase", SortOrder::ASC}}),
            10));
    case 27:
        return Collect(Project(
            Limit(Sort(Filter(Scan(db_filename, {"SearchPhrase", "EventTime"}),
                              StringNotEmpty("SearchPhrase")),
                       {SortKey{"EventTime", SortOrder::ASC},
                        SortKey{"SearchPhrase", SortOrder::ASC}}),
                  10),
            {"SearchPhrase"}));
    case 28:
        return Collect(OrderedLimit(
            Filter(Group(Compute(
                             Filter(Scan(db_filename, {"CounterID", "URL"}),
                                    StringNotEmpty("URL")),
                             {StringLength("URL", "url_length")}),
                         {"CounterID"},
                         {Avg("url_length", "l"), Count("c")}),
                   GreaterEqual<int64_t>("c", 100001)),
            {SortKey{"l", SortOrder::DESC}}, 25));
    case 29:
        return Collect(OrderedLimit(
            Filter(Group(Compute(
                             Filter(Scan(db_filename, {"Referer"}),
                                    StringNotEmpty("Referer")),
                             {DomainColumn("Referer", "k"),
                              StringLength("Referer", "referer_length")}),
                         {"k"},
                         {Avg("referer_length", "l"),
                          Count("c"),
                          Min("Referer", "min_referer")}),
                   GreaterEqual<int64_t>("c", 100001)),
            {SortKey{"l", SortOrder::DESC}}, 25));
    case 30: {
        std::vector<ComputeOperator::ComputedColumnSpec> computed;
        std::vector<AggregateSpec> aggregates{Sum("ResolutionWidth", "s0")};
        computed.reserve(89);
        aggregates.reserve(90);
        for (int64_t idx = 1; idx < 90; idx++) {
            const std::string name = "rw_" + std::to_string(idx);
            computed.push_back(SmallIntPlus("ResolutionWidth", idx, name));
            aggregates.push_back(Sum(name, "s" + std::to_string(idx)));
        }
        return Collect(Global(
            Compute(Scan(db_filename, {"ResolutionWidth"}), std::move(computed)),
            std::move(aggregates)));
    }
    case 31:
        return Collect(OrderedLimit(
            Group(QueryWithSearchPhrase(
                      db_filename,
                      {"SearchEngineID", "ClientIP", "IsRefresh",
                       "ResolutionWidth", "SearchPhrase"}),
                  {"SearchEngineID", "ClientIP"},
                  {Count("c"), Sum("IsRefresh", "sum"),
                   Avg("ResolutionWidth", "avg")}),
            {SortKey{"c", SortOrder::DESC}}, 10));
    case 32:
        return Collect(OrderedLimit(
            Group(QueryWithSearchPhrase(
                      db_filename,
                      {"WatchID", "ClientIP", "IsRefresh",
                       "ResolutionWidth", "SearchPhrase"}),
                  {"WatchID", "ClientIP"},
                  {Count("c"), Sum("IsRefresh", "sum"),
                   Avg("ResolutionWidth", "avg")}),
            {SortKey{"c", SortOrder::DESC}}, 10));
    case 33:
        return Collect(OrderedLimit(
            Group(Scan(db_filename,
                       {"WatchID", "ClientIP", "IsRefresh",
                        "ResolutionWidth"}),
                  {"WatchID", "ClientIP"},
                  {Count("c"), Sum("IsRefresh", "sum"),
                   Avg("ResolutionWidth", "avg")}),
            {SortKey{"c", SortOrder::DESC}}, 10));
    case 34:
        return Collect(OrderedLimit(
            Group(Scan(db_filename, {"URL"}), {"URL"}, {Count("c")}),
            {SortKey{"c", SortOrder::DESC}}, 10));
    case 35:
        return Collect(OrderedLimit(
            Group(Compute(Scan(db_filename, {"URL"}), {ConstantOne()}),
                  {"one", "URL"}, {Count("c")}),
            {SortKey{"c", SortOrder::DESC}}, 10));
    case 36:
        return Collect(OrderedLimit(
            Group(Compute(Scan(db_filename, {"ClientIP"}),
                          {Int32Minus("ClientIP", 1, "ClientIPMinus1"),
                           Int32Minus("ClientIP", 2, "ClientIPMinus2"),
                           Int32Minus("ClientIP", 3, "ClientIPMinus3")}),
                  {"ClientIP", "ClientIPMinus1",
                   "ClientIPMinus2", "ClientIPMinus3"},
                  {Count("c")}),
            {SortKey{"c", SortOrder::DESC}}, 10));
    case 37:
        return Collect(OrderedLimit(
            Group(Filter(Scan(db_filename,
                              {"URL", "CounterID", "EventDate",
                               "DontCountHits", "IsRefresh"},
                              JulyConstraints({EqConstraint(
                                  "DontCountHits", Schema::SMALLINT,
                                  int16_t{0})})),
                         JulyFilters({EqualTo<int16_t>("DontCountHits", 0),
                                      StringNotEmpty("URL")})),
                  {"URL"}, {Count("PageViews")}),
            {SortKey{"PageViews", SortOrder::DESC}}, 10));
    case 38:
        return Collect(OrderedLimit(
            Group(Filter(Scan(db_filename,
                              {"Title", "CounterID", "EventDate",
                               "DontCountHits", "IsRefresh"},
                              JulyConstraints({EqConstraint(
                                  "DontCountHits", Schema::SMALLINT,
                                  int16_t{0})})),
                         JulyFilters({EqualTo<int16_t>("DontCountHits", 0),
                                      StringNotEmpty("Title")})),
                  {"Title"}, {Count("PageViews")}),
            {SortKey{"PageViews", SortOrder::DESC}}, 10));
    case 39:
        return Collect(OrderedLimit(
            Group(Filter(Scan(db_filename,
                              {"URL", "CounterID", "EventDate",
                               "IsRefresh", "IsLink", "IsDownload"},
                              JulyConstraints({NotEqConstraint(
                                      "IsLink", Schema::SMALLINT,
                                      int16_t{0}),
                                  EqConstraint("IsDownload", Schema::SMALLINT,
                                               int16_t{0})})),
                         JulyFilters({NotEqualTo<int16_t>("IsLink", 0),
                                      EqualTo<int16_t>("IsDownload", 0)})),
                  {"URL"}, {Count("PageViews")}),
            {SortKey{"PageViews", SortOrder::DESC}}, 10, 1000));
    case 40:
        return Collect(OrderedLimit(
            Group(Compute(Filter(
                              Scan(db_filename,
                                   {"TraficSourceID", "SearchEngineID",
                                    "AdvEngineID", "Referer", "URL",
                                    "CounterID", "EventDate", "IsRefresh"},
                                   JulyConstraints()),
                              JulyFilters()),
                          {CaseSource(), UrlDestination()}),
                  {"TraficSourceID", "SearchEngineID", "AdvEngineID",
                   "Src", "Dst"},
                  {Count("PageViews")}),
            {SortKey{"PageViews", SortOrder::DESC}}, 10, 1000));
    case 41:
        return Collect(OrderedLimit(
            Group(Filter(Scan(db_filename,
                              {"URLHash", "EventDate", "CounterID",
                               "IsRefresh", "TraficSourceID", "RefererHash"},
                              JulyConstraints({BetweenConstraint(
                                      "TraficSourceID", Schema::SMALLINT,
                                      int16_t{-1}, int16_t{6}),
                                  EqConstraint("RefererHash", Schema::BIGINT,
                                               int64_t{3594120000172545465})})),
                         JulyFilters({InSmallInt("TraficSourceID", -1, 6),
                                      EqualTo<int64_t>(
                                          "RefererHash",
                                          3594120000172545465)})),
                  {"URLHash", "EventDate"}, {Count("PageViews")}),
            {SortKey{"PageViews", SortOrder::DESC}}, 10, 100));
    case 42:
        return Collect(OrderedLimit(
            Group(Filter(Scan(db_filename,
                              {"WindowClientWidth", "WindowClientHeight",
                               "CounterID", "EventDate", "IsRefresh",
                               "DontCountHits", "URLHash"},
                              JulyConstraints({EqConstraint(
                                      "DontCountHits", Schema::SMALLINT,
                                      int16_t{0}),
                                  EqConstraint("URLHash", Schema::BIGINT,
                                               int64_t{2868770270353813622})})),
                         JulyFilters({EqualTo<int16_t>("DontCountHits", 0),
                                      EqualTo<int64_t>(
                                          "URLHash",
                                          2868770270353813622)})),
                  {"WindowClientWidth", "WindowClientHeight"},
                  {Count("PageViews")}),
            {SortKey{"PageViews", SortOrder::DESC}}, 10, 10000));
    case 43:
        return Collect(OrderedLimit(
            Group(Compute(
                      Filter(Scan(db_filename,
                                  {"EventTime", "CounterID", "EventDate",
                                   "IsRefresh", "DontCountHits"},
                                  Constraints({
                                      EqConstraint("CounterID",
                                                   Schema::INTEGER,
                                                   int32_t{62}),
                                      BetweenConstraint(
                                          "EventDate", Schema::DATE,
                                          Date("2013-07-14"),
                                          Date("2013-07-15")),
                                      EqConstraint("IsRefresh",
                                                   Schema::SMALLINT,
                                                   int16_t{0}),
                                      EqConstraint("DontCountHits",
                                                   Schema::SMALLINT,
                                                   int16_t{0})})),
                             And({EqualTo<int32_t>("CounterID", 62),
                                  GreaterEqual<int32_t>(
                                      "EventDate", Date("2013-07-14")),
                                  LessEqual<int32_t>(
                                      "EventDate", Date("2013-07-15")),
                                  EqualTo<int16_t>("IsRefresh", 0),
                                  EqualTo<int16_t>("DontCountHits", 0)})),
                      {DateTruncMinute("EventTime", "M")}),
                  {"M"}, {Count("PageViews")}),
            {SortKey{"M", SortOrder::ASC}}, 10, 1000));
    default:
        throw std::invalid_argument("ClickBench query id must be in range 1..43");
    }
}
