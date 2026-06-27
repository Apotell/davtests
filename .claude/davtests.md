# davtests — Test Harness Overview

**Repo:** `davtests` (separate repository from hlc)

**Purpose:** Runs C++ regression tests against the installed hlc library. Each `test_*.cpp` under `hlc/` becomes a separate test executable.

## Directory layout

```
davtests/
  build/           → symlink to hlc install dir
  hlc/             → test_*.cpp source files (nested to any depth)
  third_party/
    googletest/    → GTest as a git submodule
  scripts/         → Python helpers (extract.py, regression.py)
  CMakeLists.txt
```

`build/` is a symlink pointing to the hlc install directory (e.g., `hlc/out/install`). This directory is expected to contain `lib/cmake/hlc/HLCConfig.cmake`.

## CMakeLists.txt design

- GTest sourced from `third_party/googletest` submodule (`add_subdirectory` with `EXCLUDE_FROM_ALL`).
- `find_package(HLC REQUIRED HINTS "${BUILD_DIR}")` — scoped hint, does not mutate `CMAKE_PREFIX_PATH`.
- `file(GLOB_RECURSE ... CONFIGURE_DEPENDS "hlc/test_*.cpp")` — finds test sources at any nesting depth.
- Each test source builds into one executable linked against:
  - `hlc::hlc` (transitively brings in all HLC deps)
  - `GTest::gtest`, `GTest::gmock`, `GTest::gtest_main`
  - `Threads::Threads`
  - `dl` on Linux only
- `Test.cpp` is NOT compiled here — it ships in the hlc install under `src/` and is compiled as part of `hlc::hlc`.

## Updating the build symlink

After rebuilding and reinstalling hlc:
```
# Windows (run as admin or in developer shell)
mklink /D davtests\build <path-to-hlc>\out\install

# Linux
ln -sfn <path-to-hlc>/out/install davtests/build
```

## Requirements

- hlc install must provide a machine-independent `HLCConfig.cmake` (see [cmake_install_patterns.md](cmake_install_patterns.md)).
- antlr4 must be a PRIVATE dependency in the hlc build so it does not appear in installed targets.

## Setting up for the first time

```bash
git submodule add https://github.com/google/googletest third_party/googletest
cmake -G Ninja -S . -B out/build
cmake --build out/build
```
