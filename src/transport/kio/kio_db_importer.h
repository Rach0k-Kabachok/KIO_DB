#pragma once

#include <string>

#include "schema.h"
#include "transport/csv/csv_columnar_reader.h"
#include "transport/kio/kio_db_writer.h"

class KioDbImporter {
    CSVColumnarReader csv_reader_;
    KioDbWriter& writer_;


public:
    KioDbImporter(const std::string& csv_filename, const Schema& schema,
          KioDbWriter& writer);

    KioDbImporter(const KioDbImporter&) = delete;
    KioDbImporter& operator=(const KioDbImporter&) = delete;

    KioDbImporter(KioDbImporter&&) = delete;
    KioDbImporter& operator=(KioDbImporter&&) = delete;

    void Import();
    
};
