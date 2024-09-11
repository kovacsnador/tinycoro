project(tinycoro_tests) # name of your project


set(GOOGLE_TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../vendor/googletest)

add_subdirectory(${GOOGLE_TEST_DIR} ${CMAKE_CURRENT_BINARY_DIR}/test)    # add googletest subdirectory

add_executable(${PROJECT_NAME})

target_include_directories(${PROJECT_NAME}
    PRIVATE
    ${GOOGLE_TEST_DIR}/googletest/include
    ${GOOGLE_TEST_DIR}/googlemock/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
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