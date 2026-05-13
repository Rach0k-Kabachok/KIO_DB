#include "kio_work/kio_db.h"

#include <string>

#include "csv_work/csv_batch_reader.h"
#include "csv_work/csv_formatter.h"
#include "kio_work/kio_db_writer.h"
#include "schema.h"

namespace {
Schema LoadSchema(const std::string& schema_filename) {
    CSVBatchReader schema_reader(schema_filename);
    ctp::ParsedBatch schema_rows;
    ctp::ParsedBatch batch;

    while (!(batch = schema_reader.ParseNextBatch()).empty()) {
        schema_rows.insert(schema_rows.end(), batch.begin(), batch.end());
    }

    return Schema(schema_rows);
}
}  // namespace

KioDb::KioDb(const std::string& csv_filename, const std::string& schema_filename,
             const std::string& output_db_name)
        : schema_(LoadSchema(schema_filename)),
          csv_formatter_(csv_filename, schema_),
          writer_(output_db_name, schema_) {
}

void KioDb::ProcessAllInput() {
    while (ReadNextBatch()) {
        WriteBatchToKIO();
    }
}

bool KioDb::ReadNextBatch() {
    batch_ = csv_formatter_.MakeColumnarBatch();
    return !batch_.empty();
}

void KioDb::WriteBatchToKIO() {
    writer_.WriteBatchToFile(batch_);
}
