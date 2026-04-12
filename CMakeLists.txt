# Specify the minimum version for CMake
cmake_minimum_required(VERSION 3.10)

# Project's name
project(tinycoro_example)

# Always generate compile_commands.json so editors can discover the real
# compiler flags, include paths, and C++ standard for IntelliSense.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Register tests in the top-level build tree so `ctest` works from `build/`.
enable_testing()

# Set the C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Header-only, INTERFACE target for dependents
add_library(tinycoro INTERFACE)
add_library(tinycoro::tinycoro ALIAS tinycoro)
target_include_directories(tinycoro INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(tinycoro INTERFACE cxx_std_20)
find_package(Threads REQUIRED)
target_link_libraries(tinycoro INTERFACE Threads::Threads)

# Option to enable ASAN
option(WithASAN "Build with AddressSanitizer" OFF)
option(WithTSAN "Build with ThreadSanitizer" OFF)
option(BUILD_WITH_EXAMPLES "Build with Examples" ON)
option(BUILD_WITH_DIAGNOSTICS "Build with diagnostics" OFF)
option(TINYCORO_BUILD_TESTS "Build tinycoro unit tests" ON)

if (WithASAN AND WithTSAN)
  message(FATAL_ERROR "WithASAN and WithTSAN cannot be enabled together")
endif()

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
  set(CMAKE_CXX_FLAGS                     "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -Wno-interference-size") # disable warning for std::hardware_destructive_interference_size
  set(CMAKE_CXX_FLAGS_DEBUG               "${CMAKE_CXX_FLAGS} -O0 -g")
  
  # GCC 11.x has linker issues with O3, use O0 for release builds.
  # GitHub Actions typically reports versions like 11.4.0, so match the
  # whole GCC 11 series instead of only the exact string "11".
  if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "11"
      AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "12")
    set(CMAKE_CXX_FLAGS_RELEASE           "${CMAKE_CXX_FLAGS} -O0")
  else()
    set(CMAKE_CXX_FLAGS_RELEASE           "${CMAKE_CXX_FLAGS} -O3")
  endif()
endif()

if (WithASAN)
    message("Setting sanitizers...")
    add_compile_options(-fsanitize=address -fsanitize=undefined -fsanitize=leak)
    add_link_options(-fsanitize=address -fsanitize=undefined -fsanitize=leak)

    set(ENV{LSAN_OPTIONS} "verbosity=2:log_threads=1")
endif()

if (WithTSAN)
  message("Setting ThreadSanitizer...")
  add_compile_options(-fsanitize=thread -fno-omit-frame-pointer -g -O0)
  add_link_options(-fsanitize=thread)
endif()

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  message("CMAKE_CXX_FLAGS_DEBUG -> ${CMAKE_CXX_FLAGS_DEBUG}")
elseif (CMAKE_BUILD_TYPE STREQUAL "Release")
  message("CMAKE_CXX_FLAGS_RELEASE -> ${CMAKE_CXX_FLAGS_RELEASE}")
endif()

if(BUILD_WITH_DIAGNOSTICS)
  # add global flag for diagnostics
  add_compile_definitions(TINYCORO_DIAGNOSTICS)
endif()

if(BUILD_WITH_EXAMPLES)

# Add an executable with the source file
add_executable(${PROJECT_NAME} example/example.cpp)

if(UNIX)
  # on linux we also build the echo server stress test
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/example/echo_server_example)
  
  # on linux we also build the curl_example
  find_package(CURL QUIET)
  if(CURL_FOUND)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/curl_example)
  else()
    message(STATUS "libcurl not found, skipping curl_example")
  endif()
endif()

# add include directories
target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

endif()

if(TINYCORO_BUILD_TESTS)
	add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/test)
endif()
