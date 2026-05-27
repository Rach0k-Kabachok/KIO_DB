add_library(kio_core STATIC
        transport/csv/csv_row_parser.cpp
        transport/csv/csv_batch_reader.cpp
        transport/csv/csv_columnar_reader.cpp
        transport/csv/csv_exporter.cpp

        transport/compression/binary_codec.cpp
        transport/compression/bit_packing_encoding.cpp
        transport/compression/column_encoding.cpp
        transport/compression/compression_codec.cpp
        transport/compression/delta_encoding.cpp
        transport/compression/delta_length_byte_array_encoding.cpp
        transport/compression/dictionary_encoding.cpp
        transport/compression/none_compression.cpp
        transport/compression/plain_encoding.cpp
        transport/compression/rle_encoding.cpp

        transport/kio/kio_db_importer.cpp
        transport/kio/binary_io.cpp
        transport/kio/kio_db_reader.cpp
        transport/kio/kio_db_writer.cpp
        transport/kio/kio_serialization.cpp

        global/kio_db.cpp
        global/schema.cpp
        global/column_operations.cpp
        global/scalar_value.cpp

        execution/table_scan_op.cpp
        execution/predicates.cpp
        execution/query_plan_helpers.cpp
        execution/clickbench_queries.cpp
        execution/compute_op.cpp
        execution/filter_op.cpp
        execution/project_op.cpp
        execution/result_writer.cpp
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
