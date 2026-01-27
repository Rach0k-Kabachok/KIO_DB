cmake_minimum_required(VERSION 3.22)
project(KIO_DB)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror -Wpedantic")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-parameter -Wno-unused-function")


set(HEADERS
        src/csv_row_parser.h
        src/csv_batch_reader.h
        src/csv_foramtter.h
        src/schema.h
        src/kio_db.h
)

set(SOURCES
        src/main.cpp
)

add_executable(kio_db ${SOURCES} ${HEADERS})

target_include_directories(kio_db PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)


# -------------------- tests --------------------
include(FetchContent)

FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG release-1.12.1
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

enable_testing()
add_subdirectory(tests)
