add_executable(kio_db_tests
        example_test.cpp
)

target_link_libraries(kio_db_tests PRIVATE
        GTest::gtest_main
)

target_include_directories(kio_db_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

include(GoogleTest)
gtest_discover_tests(kio_db_tests)