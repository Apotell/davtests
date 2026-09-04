---
name: writing-hlc-tests
description: Write or review GTest C++ tests for the davtests suite that exercise HLC/HLDB against SystemVerilog source. Use when adding a test_<name>.cpp for a .hlc/.sv test case, asserting on UHDM/hldb objects (Module, SequenceDecl, PropertyDecl, Assert, Operation, RefObj), checking HLC diagnostics with findError(), or when a test needs to encode what IEEE 1800 requires rather than what HLC currently outputs.
---

# Writing davtests tests

## Core principle: assert the standard, not the implementation

**Every assertion must encode what the IEEE 1800 standard requires, never what HLC
currently outputs.** Assume the implementation is doing it all wrong until you have
verified the behavior against the standard text itself. Do not treat existing tool
output, an existing passing test, or a prior test author's comment as a source of
truth. If unsure what the standard requires, read the actual grammar/section text
(do not rely on memory), or ask before writing the assertion.

This matters because it is easy to write a test that locks in a bug: run the
compiler, see what it produced, and assert exactly that. Such a test passes today
and keeps passing after a refactor that preserves the bug, but it actively resists
ever being fixed -- a correct fix makes a "passing" test start failing, which reads
as a regression instead of a correction. This has happened repeatedly in this suite:
net-vs-variable modeling was asserted against whatever HLC happened to produce for
structs/arrays/typedef'd declarations (should have been asserted per IEEE 1800-2023
Sec 6.7/6.8: no net-type keyword means variable, regardless of `` `default_nettype ``),
and attribute attachment was asserted as hoisted onto the module instead of attached
to the individual declaration (Sec 5.12). In every case the fix was to rewrite the
assertions to match the standard, not to "fix" the standard-based test to match the
tool.

## Hard rules

### Known limitations: skip, never assert the gap

**For a known limitation, bug, or missing feature: never assert on what's missing --
that's useless.** Writing `EXPECT_EQ(x, nullptr)` because the field happens to come
back empty today just locks the gap in under a different disguise. Instead assert
what the standard actually requires, and mark the test skipped with proper reasoning
as the very first line of the test body:

```cpp
GTEST_SKIP() << "HLC does <wrong thing>; should <standard-correct thing> per IEEE 1800-2023 Sec <N>. Fix pending.";
```

so the gap is visible as pending work rather than silently passing against wrong
output, or failing and being mistaken for flakiness.

**When a newly found limitation/bug/missing-feature isn't local to one test file**
(a systemic gap likely to recur elsewhere in the suite), document it in the hlc_01
repo's `.claude/instructions/` folder (e.g. `library_discrepancies.md` or a new file)
rather than leaving it buried in a single `GTEST_SKIP()` message.

### Don't write tests from the .log dump

**Except in exceptional cases, don't base tests on what the `.log` dump produces --
avoid reading the log to write tests at all.** A log only captures what the tool
*currently* does, which is exactly the same "locks in a bug" trap as trusting tool
output directly. Use the headers under `build/include/hldb/` to understand the object
model and API surface instead. Reading the log is only appropriate for narrow,
exceptional confirmations (e.g. the exact wording/location of a diagnostic whose text
isn't standard-mandated) -- never as the primary basis for what shape a test should
expect.

### Never call getFullName() / assert on vpiFullName

It's a computed property that is currently wrong in HLC. Use `getName()` only.

### Don't count errors/warnings -- use findError()

`EXPECT_EQ(stats.nbError, 0)` / `EXPECT_EQ(stats.nbWarning, 0)` (via
`ErrorContainer::getErrorStats()`) is too broad and brittle -- it breaks the moment
error/warning reporting improves elsewhere (e.g. a previously-silent case starts
correctly flagging something unrelated to what this file is testing), turning a
genuine improvement into an apparent regression in an unrelated test. The `Test`
fixture (`hlc/Tests/Test.h`) provides `findError()` with four overloads:

```cpp
const Error *findError(ErrorDefinition::ErrorType type);
const Error *findError(ErrorDefinition::ErrorType type, std::string_view symbol);
const Error *findError(ErrorDefinition::ErrorType type, uint32_t line, uint16_t column);
const Error *findError(ErrorDefinition::ErrorType type, std::string_view symbol, uint32_t line, uint16_t column);
```

Prefer the overloads that narrow by `symbol` and/or `line`/`column` over the bare
`type`-only one -- confirming an error is about the *specific* named object/location
under test is what actually verifies the right thing failed, not just that some error
of that type exists anywhere in the file. `findError()` returns `nullptr` when nothing
matches, so the idiomatic patterns are:

```cpp
ASSERT_NE(findError(ErrorDefinition::COMP_ILLEGAL_DEFAULT_PORT_VALUE, "port_name"), nullptr);  // error IS expected
EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "some_name"), nullptr);              // error is NOT expected
```

`EXPECT_EQ(stats.nbError, 0)` is still acceptable as a coarse top-level sanity check
when a file genuinely expects zero diagnostics of any kind, but wherever a *specific*
error is (or must not be) present, use `findError()` instead of counting.

### Don't touch the diagnostics themselves

**`ObjectBinder`'s error-reporting logic and `ErrorDefinition` entries are a
teammate's area -- don't add, extend, or "fix" the underlying diagnostics
unprompted.** Writing a `findError()`-based test assertion against existing
diagnostics is fine (that is exactly what this API is for); changing when/whether HLC
itself raises a given error is not your call to make unprompted, including the
still-outstanding sweep of ~34 `Google/chapter-7` files that check the now-dead
`ErrorDefinition::ELAB_ILLEGAL_IMPLICIT_NET` instead of its replacement
`COMP_FAILED_TO_BIND`. If you find a problem in the diagnostics themselves, flag it
(as a finding/doc note) rather than fixing it; wait for explicit go-ahead to
implement, and check whether it's already being handled first.

### Use named vpiXXX constants, never magic numbers

These enumerants are defined in `vpi_user.h` (IEEE-standard constants),
`sv_vpi_user.h` (SV extensions), and `hldb_vpi_user.h` (HLDB-specific extensions) --
include whichever one declares the constant you need instead of hardcoding its
integer value. Write `EXPECT_EQ(net->getNetType(), vpiWire)`, not
`EXPECT_EQ(net->getNetType(), 1)`; write `EXPECT_EQ(op->getOpType(), vpiMinusOp)`,
not a raw opcode number -- applies to op types, const types, net types, directions,
array types, and every other VPI enumerant. See
`hlc/Google/chapter-11/11.7--unsigned_func/test_11.7_unsigned_func.cpp` for a file
that consistently does this throughout.

## Layout

Each test targets one SystemVerilog source file under `tests/`. A test consists of
two files placed under `hlc/<Group>/<testname>/`:

| File | Purpose |
|---|---|
| `<testname>.hlc` | hlc command line (one line) |
| `test_<testname>.cpp` | GTest fixture + TEST_F cases |

The `.hlc` file and the SV file under `tests/` must already exist before writing a
test. The `.hlc` file typically looks like:

```
-wd ../../../tests/<Group> -parse -d db -d ast <testname>.sv -nobuiltin
```

## Workflow

### Step 1 -- Read the SV source and grammar

Read the `.sv` file under `tests/` to understand what SystemVerilog constructs are
being tested. Then cross-reference `grammar/SV3_1aParser.g4` to understand the parse
structure HLC will build. The grammar tells you which rules fire for a given
construct -- e.g. `sequence_declaration`, `property_declaration`,
`concurrent_assertion_statement` -- and therefore which UHDM objects will be created.
Trace how the SV constructs map to UHDM object types.

### Step 2 -- Map grammar constructs to UHDM objects and C++ API

The UHDM object model is generated from YAML files; headers live in
`build/include/hldb/`. **Always read the actual header before using an API -- never
guess method names.** See `references/uhdm-api.md` for the construct-to-object
mappings, the UHDM-field-to-method naming rule, common includes, key constants, and
the inheritance table.

### Step 3 -- Write the fixture

Follow this exact skeleton (copy from an existing test):

```cpp
namespace hlc {
class MyTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "mytestname.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};
// ... All tests belonging to MyTest go here!
} // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
```

`m_session`, `m_compiler`, and `m_design` are static members inherited from `Test`.

### Step 4 -- Write the test cases

Typical progression per feature:

1. **Existence** -- the module/decl/assertion is present and non-null.
2. **Content** -- the key field (expression, clocking event, property expr) is non-null.
3. **Shape** -- opType, operand count, or referenced name matches what the grammar dictates.
4. **Absence** -- confirm that something is NOT present (e.g. no `PropertyDecl` when the assert is inline).

Pattern for finding a named decl:

```cpp
const hldb::SequenceDecl *seq = nullptr;
for (const hldb::SequenceDecl *const s : *tb->getSequenceDecls()) {
  if (s->getName() == "myseq") { seq = s; break; }
}
ASSERT_NE(seq, nullptr) << "sequence 'myseq' not found";
```

Pattern for polymorphic cast to a concrete type:

```cpp
const hldb::Assert *found = nullptr;
for (const hldb::ConcurrentAssertions *const ca : *tb->getConcurrentAssertions()) {
  if (const hldb::Assert *const a = any_cast<hldb::Assert>(ca)) {
    found = a; break;
  }
}
```

**Look beyond existence and name.** Depending on the construct under test, also check
properties like `getScalared()`/`getVectored()`, `getNetType()`, `getIsMethodCall()`,
signed/unsigned, packed/unpacked, and similar flags. A test that only confirms a node
exists and is named correctly is leaving real coverage on the table.

## Pitfalls

### Templated getXX<T>() and null checks

Be careful with templated accessors when checking for absence.
`ASSERT_EQ(ptr->getActual<Net>(), nullptr)` is true both when `getActual()` is
genuinely null AND when it's non-null but simply isn't a `Net` -- these are two
different facts, and the templated form conflates them. Avoid the templated overload
for a null check; use the plain (non-templated) accessor instead. If you need to
assert both "present" and "is specifically type T," split it into two assertions:

```cpp
ASSERT_NE(ptr->getActual(), nullptr);
EXPECT_EQ(ptr->getActual()->getAnyType(), hldb::AnyType::Net);
```

rather than relying on a single templated-downcast comparison to carry both meanings.

### Filename collisions

CMake globs all `hlc/**/test_*.cpp` files and builds one executable per file, with
output flattened by basename -- two test files with the same name in different folders
collide at CMake configure time (duplicate target name). Before naming a new test
file, check whether the basename already exists elsewhere in the tree. If a collision
would occur (e.g. `ansi/module.sv` and `nonansi/module.sv` would both naturally want
`test_5.1.2-module.cpp`), prefix the filename with the parent folder's name to
disambiguate: `test_5.1.2-ansi_module.cpp` and `test_5.1.2-nonansi_module.cpp`.

## Coding conventions

- **ASCII only.** All code and comments you add or modify must use plain ASCII
  characters (0x00-0x7F) only. Do not introduce any non-ASCII / Unicode characters -
  no smart quotes (use `'` and `"`), no em/en dashes (use `-` or `--`), no arrows
  (write `->`), no ellipsis character (write `...`), no non-breaking spaces, and no
  Unicode box-drawing or symbol characters in comments. This applies to source,
  headers, tests, and inline comments.
- **Container insertion.** Prefer `emplace` / `emplace_back` over `insert` /
  `push_back` when adding elements to standard containers.
- **Explicit types.** Prefer explicit type names over `auto`. Only use `auto` when
  the type is genuinely verbose or already stated in the same expression (e.g. the
  result of a cast or `make_shared`).
- **Member naming.** Prefix all struct and class data members with `m_` (e.g.
  `m_fileId`, `m_startLine`).
- **Test class naming.** Test class names must be suffixed with `Test` (e.g.
  `class InterfaceIdentifiersTest : public Test`).
- **Test file naming and placement.** Test files must be named `test_<test-name>.cpp`
  and placed in the same directory as the corresponding `<test-name>.hlc` file.
- **clang-format.** All authored or modified C++ code must conform to the project
  `.clang-format` in the repository root. Run `clang-format` on any `.cpp` or `.h`
  file you add or change before committing.
- **License header.** Every new `.cpp` file must begin with the Apache 2.0 header
  attributed to **Apotell** (see `CLAUDE.md`).
