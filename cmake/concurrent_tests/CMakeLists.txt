cmake_minimum_required(VERSION 3.5)

project(concurrent_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 98)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(concurrent_tests
    ../../SPSCAudio.cpp
    ../../my_concurrent.h
    ../../my_spsc_buffer.h)

#set_target_properties(concurrent_tests PROPERTIES COMPILE_FLAGS "-m32" LINK_FLAGS "-m32")

Target_link_libraries( concurrent_tests winmm)
