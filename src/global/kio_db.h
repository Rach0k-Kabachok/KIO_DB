#include "global/schema.h"
#include <string>

class KioDb {
    Schema schema_;
    std::string csv_in_file_;
    std::string kio_db_file_;

public:
    KioDb(const std::string& schema_file, const std::string& csv_in_file, 
          const std::string& kio_db_file);
    
    void ImportCsvToKio();
    void ExportKioToCsv(const std::string& csv_out_file);
};
