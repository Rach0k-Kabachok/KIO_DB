add_library(kio_global STATIC
        global/kio_db.cpp
        global/schema.cpp
        global/column_operations.cpp
        global/scalar_value.cpp
)

target_include_directories(kio_global PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/global
)

add_library(kio_transport STATIC
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
)

target_link_libraries(kio_transport PUBLIC kio_global)

add_library(kio_execution STATIC
        execution/operators/table_scan_op.cpp
        execution/operators/compute_op.cpp
        execution/operators/filter_op.cpp
        execution/operators/project_op.cpp
        execution/operators/result_writer_op.cpp
        execution/operators/limit_op.cpp
        execution/operators/aggregate_operator_base.cpp
        execution/operators/global_agr_op.cpp
        execution/operators/group/compact_group_table.cpp
        execution/operators/group/group_key_utils.cpp
        execution/operators/group/group_aggregate_accumulator.cpp
        execution/operators/group_agr_op.cpp
        execution/operators/sort_operator_base.cpp
        execution/operators/sort_op.cpp
        execution/operators/top_k_op.cpp

        execution/query_executor/predicates.cpp
        execution/query_executor/query_plan_helpers.cpp
        execution/query_executor/clickbench_expressions.cpp
        execution/query_executor/clickbench_filters.cpp
        execution/query_executor/clickbench_queries.cpp
        execution/query_executor/clickbench_runner.cpp
)

target_link_libraries(kio_execution PUBLIC
        kio_global
        kio_transport
)

add_library(kio_core INTERFACE)
target_link_libraries(kio_core INTERFACE
        kio_global
        kio_transport
        kio_execution
)

add_executable(kio_db main.cpp)
target_link_libraries(kio_db PRIVATE kio_core)
