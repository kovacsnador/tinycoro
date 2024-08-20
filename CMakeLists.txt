# Specify the minimum version for CMake
cmake_minimum_required(VERSION 3.10)

# Project's name
project(tinycoro_example)

# Set the C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Add an executable with the source file
add_executable(${PROJECT_NAME} example/example.cpp)

# add include directories
target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)