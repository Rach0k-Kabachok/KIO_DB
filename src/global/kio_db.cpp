#include "kio_db.h"
#include "transport/csv/csv_exporter.h"
#include "transport/kio/kio_db_importer.h"
#include "transport/kio/kio_db_reader.h"
#include "transport/kio/kio_db_writer.h"

KioDb::KioDb(const std::string& schema_file, const std::string& csv_in_file, 
          const std::string& kio_db_file):
          schema_(schema_file),
          csv_in_file_(csv_in_file),
          kio_db_file_(kio_db_file) {
}
	    
void KioDb::ImportCsvToKio() {
    KioDbWriter writer(kio_db_file_, schema_);
    KioDbImporter importer(csv_in_file_, schema_, writer);
    importer.Import();
    writer.Finalize();
}

void KioDb::ExportKioToCsv(const std::string& csv_out_file) {
    KioDbReader reader(kio_db_file_);
    CsvExporter exporter(reader, csv_out_file);
    exporter.Export();
}
