add_executable(kio_db_tests
        csv_rw/row_parser_test.cpp
        csv_rw/batch_reader_test.cpp
        csv_rw/csv_columnar_reader_test.cpp
        csv_rw/read_write_test.cpp
        csv_rw/hits_round_trip_test.cpp
        compression/column_encoding_test.cpp
        execution/limit_op_test.cpp
        execution/aggregate_op_test.cpp
        execution/clickbench_query_test.cpp
        execution/sort_op_test.cpp
        execution/top_k_op_test.cpp
)

target_link_libraries(kio_db_tests PRIVATE kio_core GTest::gtest_main)


include(GoogleTest)
gtest_discover_tests(kio_db_tests)
