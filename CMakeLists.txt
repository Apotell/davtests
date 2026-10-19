cmake_minimum_required(VERSION 3.20)
project(davtests CXX)

# Setup and helper functions shared with the per-folder CMakeLists.txt files.
include(cmake/DavTests.cmake)

# Every test_*.cpp under hlc/ builds into its own executable, except below a
# folder that has its own CMakeLists.txt: that file decides how its folder is
# built (see cmake/DavTests.cmake).
davtests_add_tree("${DAVTESTS_HLC_DIR}")
