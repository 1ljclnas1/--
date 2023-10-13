cmake_minimum_required(VERSION 3.25)
project(tearCode)

set(CMAKE_CXX_STANDARD 20)

set(HEADER_FILES includes/*.h)

set(SOURCES srcs/LRU.cpp srcs/balance_number.cpp includes/balance_number.h)

add_executable(tearCode main.cpp ${SOURCES})
target_include_directories(tearCode PRIVATE includes)