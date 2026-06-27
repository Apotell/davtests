cmake_minimum_required(VERSION 3.20)
project(davtests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fix IntelliSense on Windows - add MSVC standard library paths
if(WIN32)
    include_directories(SYSTEM
        "C:/Program Files/LLVM/lib/clang/22/include"
        "C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC/14.51.36231/include"
        "C:/Program Files (x86)/Windows Kits/10/Include/10.0.26100.0/ucrt"
        "C:/Program Files (x86)/Windows Kits/10/Include/10.0.26100.0/um"
        "C:/Program Files (x86)/Windows Kits/10/Include/10.0.26100.0/shared"
    )
endif()

# GTest from submodule
if(MSVC)
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
endif()
add_subdirectory(third_party/googletest EXCLUDE_FROM_ALL)

enable_testing()
include(GoogleTest)

set(HLC_BUILD_DIR "${CMAKE_SOURCE_DIR}/build" CACHE PATH "hlc install directory")
cmake_path(ABSOLUTE_PATH HLC_BUILD_DIR BASE_DIRECTORY "${CMAKE_SOURCE_DIR}" NORMALIZE OUTPUT_VARIABLE HLC_BUILD_DIR)

find_package(HLC REQUIRED HINTS "${HLC_BUILD_DIR}")
find_package(Threads REQUIRED)

file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_SOURCE_DIR}/hlc/test_*.cpp"
  "${CMAKE_SOURCE_DIR}/hlc/*_test_*.cpp")

foreach(TEST_SRC IN LISTS TEST_SOURCES)
  get_filename_component(FILE_STEM "${TEST_SRC}" NAME_WLE)
  get_filename_component(TEST_DIR  "${TEST_SRC}" DIRECTORY)
  get_filename_component(DIR_NAME  "${TEST_DIR}" NAME)
  set(TEST_NAME "test_${DIR_NAME}")

  # Strip optional section-number prefix (e.g. "6.5_" → "test_variable_assignment")
  string(REGEX REPLACE "^[0-9]+(\\.[0-9]+)*_" "" TEST_NAME "${FILE_STEM}")

  add_executable(${TEST_NAME} ${TEST_SRC} ${HLC_BUILD_DIR}/src/Tests/Test.cpp)
  target_link_libraries(${TEST_NAME} PRIVATE
    hlc::hlc
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