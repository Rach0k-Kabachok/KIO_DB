add_executable(kio_db_tests
        csv_rw/row_parser_test.cpp
        csv_rw/batch_reader_test.cpp
        csv_rw/csv_formater.cpp
        csv_rw/red_write_test.cpp
)

target_link_libraries(kio_db_tests PRIVATE kio_core GTest::gtest_main)


include(GoogleTest)
gtest_discover_tests(kio_db_tests)
