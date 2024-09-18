project(tinycoro_tests) # name of your project

if (MSVC)
    set(CMAKE_CXX_FLAGS_DEBUG   "${CMAKE_CXX_FLAGS} /MTd") # for dynamic libs
else() 
    set(CMAKE_CXX_FLAGS                     "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
    set(CMAKE_CXX_FLAGS_DEBUG               "${CMAKE_CXX_FLAGS} -O0 -g")
    set(CMAKE_CXX_FLAGS_RELEASE             "${CMAKE_CXX_FLAGS} -O2")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO      "${CMAKE_CXX_FLAGS} -O2")
    set(CMAKE_CXX_FLAGS_MINSIZEREL          "${CMAKE_CXX_FLAGS} -O2")
endif()

set(GOOGLE_TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../vendor/googletest)

add_subdirectory(${GOOGLE_TEST_DIR} ${CMAKE_CURRENT_BINARY_DIR}/test)    # add googletest subdirectory

add_executable(${PROJECT_NAME})

target_include_directories(${PROJECT_NAME}
    PRIVATE
    ${GOOGLE_TEST_DIR}/googletest/include
    ${GOOGLE_TEST_DIR}/googlemock/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../
)

# Collect all source files
file(GLOB_RECURSE UNIT_TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

target_sources(${PROJECT_NAME}
    PRIVATE
    ${UNIT_TEST_SOURCES}
)

target_link_libraries(${PROJECT_NAME}
                      PRIVATE gtest
                      PRIVATE gmock)