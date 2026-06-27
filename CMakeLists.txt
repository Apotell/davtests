cmake_minimum_required(VERSION 3.20)
project(davtests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
endif()
add_subdirectory(third_party/googletest EXCLUDE_FROM_ALL)

enable_testing()
include(GoogleTest)

set(HLC_BUILD_DIR "${CMAKE_SOURCE_DIR}/build" CACHE PATH "hlc build directory")
cmake_path(ABSOLUTE_PATH HLC_BUILD_DIR BASE_DIRECTORY "${CMAKE_SOURCE_DIR}" NORMALIZE OUTPUT_VARIABLE HLC_BUILD_DIR)

find_package(Surelog REQUIRED HINTS "${HLC_BUILD_DIR}/lib/cmake/Surelog")
find_package(Threads REQUIRED)

file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_SOURCE_DIR}/hlc/test_*.cpp"
  "${CMAKE_SOURCE_DIR}/hlc/*_test_*.cpp")

foreach(TEST_SRC IN LISTS TEST_SOURCES)
  get_filename_component(FILE_STEM "${TEST_SRC}" NAME_WLE)
  get_filename_component(TEST_DIR  "${TEST_SRC}" DIRECTORY)

  string(REGEX REPLACE "^[0-9]+(\\.[0-9]+)*_" "" TEST_NAME "${FILE_STEM}")

  add_executable(${TEST_NAME} ${TEST_SRC} "${CMAKE_SOURCE_DIR}/src/Tests/Test.cpp")
  target_include_directories(${TEST_NAME} PRIVATE
    "${HLC_BUILD_DIR}/include"
    "${CMAKE_SOURCE_DIR}/include")
  target_link_libraries(${TEST_NAME} PRIVATE
    surelog::surelog
    GTest::gtest
    GTest::gmock
    GTest::gtest_main
    Threads::Threads)
  gtest_discover_tests(${TEST_NAME}
    TEST_PREFIX "${TEST_NAME}/"
    WORKING_DIRECTORY "${TEST_DIR}"
    DISCOVERY_TIMEOUT 60)

  if(UNIX AND NOT APPLE)
    target_link_libraries(${TEST_NAME} PRIVATE dl)
  endif()
endforeach()
