cmake_minimum_required(VERSION 3.8)
project(VSM)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES
        main.c)

add_executable(VSM ${SOURCE_FILES})