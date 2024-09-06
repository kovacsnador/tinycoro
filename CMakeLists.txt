# Specify the minimum version for CMake
cmake_minimum_required(VERSION 3.10)

# Project's name
project(tinycoro_example)

# Set the C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

if (MSVC)
    # warning level
    set(CMAKE_CXX_FLAGS         "${CMAKE_CXX_FLAGS} /W3")
    set(CMAKE_CXX_FLAGS_DEBUG   "${CMAKE_CXX_FLAGS} /MTd") # for dynamic libs
else() 
    set(CMAKE_CXX_FLAGS                     "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
    set(CMAKE_CXX_FLAGS_DEBUG               "${CMAKE_CXX_FLAGS} -O0 -g")
    set(CMAKE_CXX_FLAGS_RELEASE             "${CMAKE_CXX_FLAGS} -O2")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO      "${CMAKE_CXX_FLAGS} -O2")
    set(CMAKE_CXX_FLAGS_MINSIZEREL          "${CMAKE_CXX_FLAGS} -O2")
endif()

# Add an executable with the source file
add_executable(${PROJECT_NAME} example/example.cpp)

# add include directories
target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# build unit tests
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/test)