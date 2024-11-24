# Specify the minimum version for CMake
cmake_minimum_required(VERSION 3.10)

# Project's name
project(tinycoro_example)

# Set the C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Option to enable ASAN
option(WithASAN "Build with AddressSanitizer" OFF)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  # using Clang
  set(CMAKE_CXX_FLAGS                     "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -pthread")
  
  if (WIN32)
    # add emulated Thread-Local Storage (tls) under windows.
    set(CMAKE_CXX_FLAGS                     "${CMAKE_CXX_FLAGS} -femulated-tls")
  endif()
  
  set(CMAKE_CXX_FLAGS_DEBUG               "${CMAKE_CXX_FLAGS} -O0 -g")
  set(CMAKE_CXX_FLAGS_RELEASE             "${CMAKE_CXX_FLAGS} -O3")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  # using GCC
  set(CMAKE_CXX_FLAGS                     "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
  set(CMAKE_CXX_FLAGS_DEBUG               "${CMAKE_CXX_FLAGS} -O0 -g")
  set(CMAKE_CXX_FLAGS_RELEASE             "${CMAKE_CXX_FLAGS} -O3")
endif()

if (WithASAN)
    message("Setting sanitizers...")
    add_compile_options(-fsanitize=address, -fsanitize=undefined, -fsanitize=leak)
    add_link_options(-fsanitize=address, -fsanitize=undefined, -fsanitize=leak)

    set(ENV{LSAN_OPTIONS} "verbosity=2:log_threads=1")
endif()

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  message("CMAKE_CXX_FLAGS_DEBUG -> ${CMAKE_CXX_FLAGS_DEBUG}")
elseif (CMAKE_BUILD_TYPE STREQUAL "Release")
  message("CMAKE_CXX_FLAGS_RELEASE -> ${CMAKE_CXX_FLAGS_RELEASE}")
endif()

# Add an executable with the source file
add_executable(${PROJECT_NAME} example/example.cpp)

# add include directories
target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# build unit tests
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/test)