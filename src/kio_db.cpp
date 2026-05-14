#include "kio_db.h"
#include "transport/csv/csv_exporter.h"

KioDb::KioDb(const std::string& schema_file, const std::string& csv_in_file, 
          const std::string& kio_db_file):
          schema_(schema_file), 
          writer_(kio_db_file, schema_), 
          reader_(kio_db_file, schema_), 
          importer_(csv_in_file, schema_, writer_) {
}
    
void KioDb::ImportCsvToKio() {
    importer_.Import();
}

void KioDb::ExportKioToCsv(const std::string& csv_out_file) {
    CsvExporter exporter(reader_, csv_out_file);
    exporter.Export();
}
