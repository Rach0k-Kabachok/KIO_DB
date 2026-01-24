cmake_minimum_required(VERSION 3.22)
project(KIO_DB)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-function -Wpedantic")

add_executable(kio_db src/main.cpp)

target_include_directories(kio_db PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
