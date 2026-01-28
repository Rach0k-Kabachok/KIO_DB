add_library(kio_core STATIC
        csv_work/csv_row_parser.cpp
        csv_work/csv_batch_reader.cpp
        csv_work/csv_formatter.cpp
        csv_work/csv_exporter.cpp

        kio_work/kio_db.cpp
        kio_work/kio_db_reader.cpp
        kio_work/kio_db_writer.cpp

        schema.cpp

)

target_include_directories(kio_core PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

add_executable(kio_db main.cpp)
target_link_libraries(kio_db PRIVATE kio_core)
