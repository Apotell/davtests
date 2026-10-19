# C++ tests below this folder, built by cmake/DavTests.cmake.
#
# Modes - edit the davtests_add_folder() call at the bottom of this file:
#   BULK      one executable, bulk_Google_chapter-6, for every test_*.cpp
#             below this folder.
#   PER_FILE  one executable per test_*.cpp (same as having no CMakeLists.txt
#             here).
#
# Mixing bulk and single-file executables:
#   - Exclude files from the bulk executable; each listed file is built as its
#     own executable instead (paths relative to this folder):
#       davtests_add_folder(BULK STANDALONE test_a.cpp sub/test_b.cpp)
#   - Include a file back into the bulk executable: remove it from the
#     STANDALONE list.
#   - Build every file as its own executable:
#       davtests_add_folder(PER_FILE)
#   - Choose a different mode for one subfolder: give that subfolder its own
#     CMakeLists.txt (copy this file and fix the include() path). Files below
#     it are then built by that file, not by this one.
#   - A file whose main() is not guarded with #ifndef DAVTESTS_BULK_BUILD is
#     always built as its own executable.
# Changes are picked up on the next build (CMake reconfigures automatically).
#
# Each file in a bulk executable keeps its own ctest entry, named after the
# file; "ctest -N -V" shows which executable and --gtest_filter run it.
#
# This folder can also be configured on its own:
#   cmake -S hlc/Google/chapter-6 -B out/chapter-6
cmake_minimum_required(VERSION 3.20)
project(davtests_Google_chapter_6 CXX)
include(${CMAKE_CURRENT_LIST_DIR}/../../../cmake/DavTests.cmake)

davtests_add_folder(BULK)
