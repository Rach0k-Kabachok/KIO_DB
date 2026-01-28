#include "kio_work/kio_db.h"

#include <string>

#include "csv_work/csv_formatter.h"
#include "kio_work/kio_db_writer.h"
#include "kio_work/kio_db_reader.h"
#include "schema.h"

KioDb::KioDb(const std::string& csv_filename, const std::string& schema_filename,
             const std::string& output_db_name)
        : csv_formatter_(csv_filename, schema_filename),
          writer_(output_db_name),
          reader_(output_db_name) {
}

void KioDb::ProcessAllInput() {
    while (ReadNextBatch()) {
        WriteBatchToKIO();
    }

    writer_.Finish();
}

bool KioDb::ReadNextBatch() {
    if (schema_.IsEmpty()) {
        schema_ = csv_formatter_.GetSchema();
    }

    batch_ = csv_formatter_.MakeColumnarBatch();
    return !batch_.empty();
}

void KioDb::WriteBatchToKIO() {
    writer_.WriteBatch(batch_, schema_);
}