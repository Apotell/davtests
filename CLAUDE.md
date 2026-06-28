# davtests - Claude Code Instructions

## License header

All new source files must use the following license header:

```cpp
/*
 Copyright 2020 Apotell

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
*/
```

## Coding Conventions

- **ASCII only.** All code and comments you add or modify must use plain ASCII
  characters (0x00-0x7F) only. Do not introduce any non-ASCII / Unicode
  characters - no smart quotes (use `'` and `"`), no em/en dashes (use `-` or
  `--`), no arrows (write `->`), no ellipsis character (write `...`), no
  non-breaking spaces, and no Unicode box-drawing or symbol characters in
  comments. This applies to source, headers, tests, and inline comments.

- **Container insertion.** Always prefer `emplace` / `emplace_back` over
  `insert` / `push_back` when adding elements to standard containers.

- **Explicit types.** Prefer explicit type names over `auto`. Only use `auto`
  when the type is genuinely verbose or already stated in the same expression
  (e.g. the result of a cast or `make_shared`).

- **Member naming.** Prefix all struct and class data members with `m_`
  (e.g. `m_fileId`, `m_startLine`).

- **Test class naming.** Test class names must be suffixed with `Test`
  (e.g. `class InterfaceIdentifiersTest : public Test`).

- **Test file naming and placement.** C++ test files must be named
  `test_<test-name>.cpp` and placed in the same directory as the corresponding
  `<test-name>.hlc` file.

- **clang-format.** All authored or modified C++ code must conform to the
  repository's clang-format rules. Run `clang-format` on any `.cpp` or `.h`
  file you add or change before committing.
