#include <exception>
#include <iostream>
#include <string>

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
              << "  " << program << " export <input_kiodb> <output_csv>\n";
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
    CsvExporter exporter(reader, output_csv);
    exporter.Export();
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

        PrintUsage(argv[0]);
        return 1;
    } catch (const std::exception& error) {
        std::cerr << "kio_db: " << error.what() << '\n';
        return 1;
    }
}
