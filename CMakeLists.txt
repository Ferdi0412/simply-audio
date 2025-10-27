cmake_minimum_required(VERSION 3.12)
project(SimplyAudio)

add_library(Audio src/threads.cpp src/init.cpp)
target_include_directories(Audio PUBLIC src)
target_link_libraries(Audio PUBLIC winmm ole32 uuid)

option(BUILD_EXAMPLES "Build examples from examples/" ON)
if (BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()