#include "execution/query_executor/clickbench_queries.h"

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "execution/query_executor/clickbench_expressions.h"
#include "execution/query_executor/clickbench_filters.h"
#include "execution/query_executor/predicates.h"
#include "execution/query_executor/query_plan_helpers.h"

using namespace clickbench;
using namespace exec_plan;
using namespace exec_pred;

std::unique_ptr<IOperator> MakeClickBenchQuery(const std::string& db_filename,
                                               int query_id) {
    switch (query_id) {
    case 1:
        return Global(Scan(db_filename, {}), {Count()});
    case 2:
        return Global(
            Filter(Scan(db_filename, {"AdvEngineID"},
                        Constraints({NotEqConstraint(
                            "AdvEngineID", Schema::SMALLINT, int16_t{0})})),
                   NotEqualTo<int16_t>("AdvEngineID", 0)),
            {Count()});
    case 3:
        return Global(
            Scan(db_filename, {"AdvEngineID", "ResolutionWidth"}),
            {Sum("AdvEngineID", "sum"),
             Count(),
             Avg("ResolutionWidth", "avg")});
    case 4:
        return Global(Scan(db_filename, {"UserID"}),
                              {Avg("UserID", "avg")});
    case 5:
        return Global(Scan(db_filename, {"UserID"}),
                              {CountDistinct("UserID", "uniq")});
    case 6:
        return Global(Scan(db_filename, {"SearchPhrase"}),
                              {CountDistinct("SearchPhrase", "uniq")});
    case 7:
        return Global(Scan(db_filename, {"EventDate"}),
                              {Min("EventDate", "min"),
                               Max("EventDate", "max")});
    case 8:
        return Sort(
            Group(Filter(Scan(db_filename, {"AdvEngineID"},
                              Constraints({NotEqConstraint(
                                  "AdvEngineID", Schema::SMALLINT,
                                  int16_t{0})})),
                         NotEqualTo<int16_t>("AdvEngineID", 0)),
                  {"AdvEngineID"}, {Count("c")}),
            {SortKey{"c", SortOrder::DESC}});
    case 9:
        return OrderedLimit(
            Group(Scan(db_filename, {"RegionID", "UserID"}),
                  {"RegionID"}, {CountDistinct("UserID", "u")}),
            {SortKey{"u", SortOrder::DESC}}, 10);
    case 10:
        return OrderedLimit(
            Group(Scan(db_filename,
                       {"RegionID", "AdvEngineID", "ResolutionWidth", "UserID"}),
                  {"RegionID"},
                  {Sum("AdvEngineID", "sum"),
                   Count("c"),
                   Avg("ResolutionWidth", "avg"),
                   CountDistinct("UserID", "uniq")}),
            {SortKey{"c", SortOrder::DESC}}, 10);
    case 11:
        return OrderedLimit(
            Group(Filter(Scan(db_filename, {"MobilePhoneModel", "UserID"}),
                         StringNotEmpty("MobilePhoneModel")),
                  {"MobilePhoneModel"},
                  {CountDistinct("UserID", "u")}),
            {SortKey{"u", SortOrder::DESC}}, 10);
    case 12:
        return OrderedLimit(
            Group(Filter(Scan(db_filename,
                              {"MobilePhone", "MobilePhoneModel", "UserID"}),
                         StringNotEmpty("MobilePhoneModel")),
                  {"MobilePhone", "MobilePhoneModel"},
                  {CountDistinct("UserID", "u")}),
            {SortKey{"u", SortOrder::DESC}}, 10);
    case 13:
        return OrderedLimit(
            Group(Filter(Scan(db_filename, {"SearchPhrase"}),
                         StringNotEmpty("SearchPhrase")),
                  {"SearchPhrase"}, {Count("c")}),
            {SortKey{"c", SortOrder::DESC}}, 10);
    case 14:
        return OrderedLimit(
            Group(Filter(Scan(db_filename, {"SearchPhrase", "UserID"}),
                         StringNotEmpty("SearchPhrase")),
                  {"SearchPhrase"}, {CountDistinct("UserID", "u")}),
            {SortKey{"u", SortOrder::DESC}}, 10);
    case 15:
        return OrderedLimit(
            Group(Filter(Scan(db_filename,
                              {"SearchEngineID", "SearchPhrase"}),
                         StringNotEmpty("SearchPhrase")),
                  {"SearchEngineID", "SearchPhrase"}, {Count("c")}),
            {SortKey{"c", SortOrder::DESC}}, 10);
    case 16:
        return OrderedLimit(
            Group(Scan(db_filename, {"UserID"}), {"UserID"}, {Count("c")}),
            {SortKey{"c", SortOrder::DESC}}, 10);
    case 17:
        return OrderedLimit(
            Group(Scan(db_filename, {"UserID", "SearchPhrase"}),
                  {"UserID", "SearchPhrase"}, {Count("c")}),
            {SortKey{"c", SortOrder::DESC}}, 10);
    case 18:
        return Limit(
            Group(Scan(db_filename, {"UserID", "SearchPhrase"}),
                  {"UserID", "SearchPhrase"}, {Count("c")}),
            10);
    case 19:
        return OrderedLimit(
            Group(Compute(Scan(db_filename,
                               {"UserID", "EventTime", "SearchPhrase"}),
                          {MinuteColumn("EventTime", "m")}),
                  {"UserID", "m", "SearchPhrase"}, {Count("c")}),
            {SortKey{"c", SortOrder::DESC}}, 10);
    case 20:
        return Project(
            Filter(Scan(db_filename, {"UserID"},
                        Constraints({EqConstraint(
                            "UserID", Schema::BIGINT,
                            int64_t{435090932899640449})})),
                   EqualTo<int64_t>("UserID", 435090932899640449)),
            {"UserID"});
    case 21:
        return Global(
            Filter(Scan(db_filename, {"URL"}), Contains("URL", "google")),
            {Count()});
    case 22:
        return OrderedLimit(
            Group(Filter(Scan(db_filename, {"SearchPhrase", "URL"}),
                         And({Contains("URL", "google"),
                              StringNotEmpty("SearchPhrase")})),
                  {"SearchPhrase"}, {Min("URL", "min_url"), Count("c")}),
            {SortKey{"c", SortOrder::DESC}}, 10);
    case 23:
        return OrderedLimit(
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
            {SortKey{"c", SortOrder::DESC}}, 10);
    case 24:
        return OrderedLimit(
            Filter(Scan(db_filename, AllColumns(db_filename)),
                   Contains("URL", "google")),
            {SortKey{"EventTime", SortOrder::ASC}}, 10);
    case 25:
        return Project(
            OrderedLimit(
                Filter(Scan(db_filename, {"SearchPhrase", "EventTime"}),
                       StringNotEmpty("SearchPhrase")),
                {SortKey{"EventTime", SortOrder::ASC}}, 10),
            {"SearchPhrase"});
    case 26:
        return OrderedLimit(
            Filter(Scan(db_filename, {"SearchPhrase"}),
                   StringNotEmpty("SearchPhrase")),
            {SortKey{"SearchPhrase", SortOrder::ASC}}, 10);
    case 27:
        return Project(
            OrderedLimit(
                Filter(Scan(db_filename, {"SearchPhrase", "EventTime"}),
                       StringNotEmpty("SearchPhrase")),
                {SortKey{"EventTime", SortOrder::ASC},
                 SortKey{"SearchPhrase", SortOrder::ASC}}, 10),
            {"SearchPhrase"});
    case 28:
        return OrderedLimit(
            Filter(Group(Compute(
                             Filter(Scan(db_filename, {"CounterID", "URL"}),
                                    StringNotEmpty("URL")),
                             {StringLength("URL", "url_length")}),
                         {"CounterID"},
                         {Avg("url_length", "l"), Count("c")}),
                   GreaterEqual<int64_t>("c", 100001)),
            {SortKey{"l", SortOrder::DESC}}, 25);
    case 29:
        return OrderedLimit(
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
            {SortKey{"l", SortOrder::DESC}}, 25);
    case 30: {
        std::vector<ComputeOperator::ComputedColumnSpec> computed;
        std::vector<std::string> output_columns{"s0"};
        computed.reserve(89);
        output_columns.reserve(90);
        for (int64_t idx = 1; idx < 90; idx++) {
            const std::string name = "s" + std::to_string(idx);
            computed.push_back(
                AggregatedSumPlus("s0", "c", idx, name));
            output_columns.push_back(name);
        }
        return Project(
            Compute(Global(Scan(db_filename, {"ResolutionWidth"}),
                           {Sum("ResolutionWidth", "s0"), Count("c")}),
                    std::move(computed)),
            std::move(output_columns));
    }
    case 31:
        return OrderedLimit(
            Group(QueryWithSearchPhrase(
                      db_filename,
                      {"SearchEngineID", "ClientIP", "IsRefresh",
                       "ResolutionWidth", "SearchPhrase"}),
                  {"SearchEngineID", "ClientIP"},
                  {Count("c"), Sum("IsRefresh", "sum"),
                   Avg("ResolutionWidth", "avg")}),
            {SortKey{"c", SortOrder::DESC}}, 10);
    case 32:
        return OrderedLimit(
            Group(QueryWithSearchPhrase(
                      db_filename,
                      {"WatchID", "ClientIP", "IsRefresh",
                       "ResolutionWidth", "SearchPhrase"}),
                  {"WatchID", "ClientIP"},
                  {Count("c"), Sum("IsRefresh", "sum"),
                   Avg("ResolutionWidth", "avg")}),
            {SortKey{"c", SortOrder::DESC}}, 10);
    case 33:
        return OrderedLimit(
            Group(Scan(db_filename,
                       {"WatchID", "ClientIP", "IsRefresh",
                        "ResolutionWidth"}),
                  {"WatchID", "ClientIP"},
                  {Count("c"), Sum("IsRefresh", "sum"),
                   Avg("ResolutionWidth", "avg")}),
            {SortKey{"c", SortOrder::DESC}}, 10);
    case 34:
        return OrderedLimit(
            Group(Scan(db_filename, {"URL"}), {"URL"}, {Count("c")}),
            {SortKey{"c", SortOrder::DESC}}, 10);
    case 35:
        return OrderedLimit(
            Project(Compute(Group(Scan(db_filename, {"URL"}),
                                  {"URL"}, {Count("c")}),
                            {ConstantOne()}),
                    {"one", "URL", "c"}),
            {SortKey{"c", SortOrder::DESC}}, 10);
    case 36:
        return OrderedLimit(
            Project(Compute(Group(Scan(db_filename, {"ClientIP"}),
                                  {"ClientIP"}, {Count("c")}),
                            {Int32Minus("ClientIP", 1, "ClientIPMinus1"),
                             Int32Minus("ClientIP", 2, "ClientIPMinus2"),
                             Int32Minus("ClientIP", 3, "ClientIPMinus3")}),
                    {"ClientIP", "ClientIPMinus1", "ClientIPMinus2",
                     "ClientIPMinus3", "c"}),
            {SortKey{"c", SortOrder::DESC}}, 10);
    case 37:
        return OrderedLimit(
            Group(Filter(Scan(db_filename,
                              {"URL", "CounterID", "EventDate",
                               "DontCountHits", "IsRefresh"},
                              JulyConstraints({EqConstraint(
                                  "DontCountHits", Schema::SMALLINT,
                                  int16_t{0})})),
                         JulyFilters({EqualTo<int16_t>("DontCountHits", 0),
                                      StringNotEmpty("URL")})),
                  {"URL"}, {Count("PageViews")}),
            {SortKey{"PageViews", SortOrder::DESC}}, 10);
    case 38:
        return OrderedLimit(
            Group(Filter(Scan(db_filename,
                              {"Title", "CounterID", "EventDate",
                               "DontCountHits", "IsRefresh"},
                              JulyConstraints({EqConstraint(
                                  "DontCountHits", Schema::SMALLINT,
                                  int16_t{0})})),
                         JulyFilters({EqualTo<int16_t>("DontCountHits", 0),
                                      StringNotEmpty("Title")})),
                  {"Title"}, {Count("PageViews")}),
            {SortKey{"PageViews", SortOrder::DESC}}, 10);
    case 39:
        return OrderedLimit(
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
            {SortKey{"PageViews", SortOrder::DESC}}, 10, 1000);
    case 40:
        return OrderedLimit(
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
            {SortKey{"PageViews", SortOrder::DESC}}, 10, 1000);
    case 41:
        return OrderedLimit(
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
            {SortKey{"PageViews", SortOrder::DESC}}, 10, 100);
    case 42:
        return OrderedLimit(
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
            {SortKey{"PageViews", SortOrder::DESC}}, 10, 10000);
    case 43:
        return OrderedLimit(
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
            {SortKey{"M", SortOrder::ASC}}, 10, 1000);
    default:
        throw std::invalid_argument("ClickBench query id must be in range 1..43");
    }
}
