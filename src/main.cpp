#include "kio_db.h"

int main(int argc, const char* argv[]) {
    KioDb kiodb(argv[0], argv[1], argv[2]);
    kiodb.ImportCsvToKio();
    kiodb.ExportKioToCsv("foo.csv");
    return 0;
}