# Specify the minimum version for CMake
cmake_minimum_required(VERSION 3.10)

# Project's name
project(tinycoro_echo_server2)

# Add an executable with the source file
add_executable(${PROJECT_NAME} main.cpp)

# add include directories
target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)