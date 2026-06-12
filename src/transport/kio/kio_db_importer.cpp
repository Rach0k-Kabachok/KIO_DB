#include "transport/kio/kio_db_importer.h"
#include "columnar_types.h"
#include "schema.h"

KioDbImporter::KioDbImporter(const std::string& csv_filename,
                             const Schema& schema,
                             KioDbWriter& writer):
                             csv_reader_(csv_filename, schema), 
                             writer_(writer) {
}

void KioDbImporter::Import() {
    ctp::ColumnarBatch batch;
    while (!(batch = csv_reader_.MakeColumnarBatch()).empty()) {
        writer_.WriteBatchToFile(batch);
    }
}
        
