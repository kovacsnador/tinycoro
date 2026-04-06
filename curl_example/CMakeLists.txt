cmake_minimum_required(VERSION 3.16)

project(curl_example LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(CURL REQUIRED)

add_executable(curl_example
    main.cpp
)

target_include_directories(curl_example PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_link_libraries(curl_example PRIVATE
    CURL::libcurl
)
