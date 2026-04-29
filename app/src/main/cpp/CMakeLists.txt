cmake_minimum_required(VERSION 3.22.1)
project("signature_check")

add_library(signature_check SHARED signature_check.cpp)

target_compile_options(signature_check PRIVATE -Wall)
set_target_properties(signature_check PROPERTIES CXX_STANDARD 17)

find_library(log-lib log)
target_link_libraries(signature_check ${log-lib})
