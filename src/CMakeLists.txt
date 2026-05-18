add_library(kio_core STATIC
        transport/csv/csv_row_parser.cpp
        transport/csv/csv_batch_reader.cpp
        transport/csv/csv_columnar_reader.cpp
        transport/csv/csv_exporter.cpp

        transport/kio/kio_db_importer.cpp
        transport/kio/kio_db_reader.cpp
        transport/kio/kio_db_writer.cpp
        transport/kio/kio_serialization.cpp

        global/kio_db.cpp
        global/schema.cpp
        global/columnar_batch.cpp
        global/column_operations.cpp

        execution/table_scan_op.cpp
        execution/filter_op.cpp
        execution/project_op.cpp
        execution/limit_op.cpp
        execution/aggregate_operator_base.cpp
        execution/global_agr_op.cpp
        execution/group_agr_op.cpp
        execution/sort_operator_base.cpp
        execution/sort_op.cpp
        execution/top_k_op.cpp

)

target_include_directories(kio_core PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/global
)

add_executable(kio_db main.cpp)
target_link_libraries(kio_db PRIVATE kio_core)
