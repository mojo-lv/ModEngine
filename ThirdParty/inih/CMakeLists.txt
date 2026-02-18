cmake_minimum_required(VERSION 3.10)
project(inih)
add_library(inih STATIC ini.c cpp/INIReader.cpp)
target_include_directories(inih PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
