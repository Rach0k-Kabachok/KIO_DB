#include <exception>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

#include "execution/operators/result_writer_op.h"
#include "execution/query_executor/clickbench_queries.h"
#include "global/schema.h"
#include "transport/csv/csv_exporter.h"
#include "transport/kio/kio_db_importer.h"
#include "transport/kio/kio_db_reader.h"
#include "transport/kio/kio_db_writer.h"

namespace {
void PrintUsage(const char* program) {
    std::cerr << "Usage:\n"
              << "  " << program
              << " convert <input_csv> <schema_csv> <output_kiodb>\n"
              << "  " << program << " export <input_kiodb> <output_csv>\n"
              << "  " << program << " query <input_kiodb> <query_id> <output_csv>\n";
}

std::optional<int> ParseQueryId(const std::string& query_id) {
    try {
        size_t parsed_size = 0;
        int result = std::stoi(query_id, &parsed_size);
        if (parsed_size != query_id.size()) {
            return std::nullopt;
        }
        return result;
    } catch (const std::invalid_argument&) {
        return std::nullopt;
    } catch (const std::out_of_range&) {
        return std::nullopt;
    }
}

int ConvertCsvToKio(const std::string& input_csv,
                    const std::string& schema_csv,
                    const std::string& output_kiodb) {
    Schema schema(schema_csv);
    KioDbWriter writer(output_kiodb, schema);
    KioDbImporter importer(input_csv, schema, writer);
    importer.Import();
    writer.Finalize();
    return 0;
}

int ExportKioToCsv(const std::string& input_kiodb,
                   const std::string& output_csv) {
    KioDbReader reader(input_kiodb);
    CsvExporter exporter(output_csv);
    exporter.ExportFile(reader);
    return 0;
}

int ExecuteQuery(const std::string& input_kiodb,
                 const std::string& query_id,
                 const std::string& output_csv) {
    const std::optional<int> parsed_query_id = ParseQueryId(query_id);
    if (!parsed_query_id.has_value()) {
        std::cerr << "Invalid query id: " << query_id << "\n";
        return 1;
    }

    ResultWriterOperator writer(
        MakeClickBenchQuery(input_kiodb, *parsed_query_id), output_csv);
    writer.Next();
    return 0;
}
}  // namespace

int main(int argc, const char* argv[]) {
    try {
        if (argc < 2) {
            PrintUsage(argv[0]);
            return 1;
        }

        const std::string command = argv[1];
        if (command == "convert") {
            if (argc != 5) {
                PrintUsage(argv[0]);
                return 1;
            }
            return ConvertCsvToKio(argv[2], argv[3], argv[4]);
        }

        if (command == "export") {
            if (argc != 4) {
                PrintUsage(argv[0]);
                return 1;
            }
            return ExportKioToCsv(argv[2], argv[3]);
        }

        if (command == "query") {
            if (argc != 5) {
                PrintUsage(argv[0]);
                return 1;
            }
            return ExecuteQuery(argv[2], argv[3], argv[4]);
        }

        PrintUsage(argv[0]);
        return 1;
    } catch (const std::exception& error) {
        std::cerr << error.what() << "\n";
        return 1;
    }
}
