# app/src/main/cpp/CMakeLists.txt
cmake_minimum_required(VERSION 3.18.1)

project("xsecure")

add_library(
        xsecure
        SHARED
        xsecure.cpp
)

find_library(log-lib log)

target_link_libraries(
        xsecure
        ${log-lib}
)
