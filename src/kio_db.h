#include "schema.h"
#include "transport/kio/kio_db_writer.h"
#include "transport/kio/kio_db_reader.h"
#include "transport/kio/kio_db_importer.h"

class KioDb {
    Schema schema_;
    KioDbWriter writer_;
    KioDbReader reader_;
    KioDbImporter importer_;

public:
    KioDb(const std::string& schema_file, const std::string& csv_in_file, 
          const std::string& kio_db_file);
    
    void ImportCsvToKio();
    void ExportKioToCsv(const std::string& csv_out_file);
};