# davtests -- Test Writing Guide

## Core principle: assert the standard, not the implementation

**Strong recommendation: every assertion must encode what the IEEE 1800 standard requires, never what HLC currently outputs.** Assume the implementation is doing it all wrong until you have verified the behavior against the standard text itself -- do not treat existing tool output, an existing passing test, or a prior test author's comment as a source of truth. If you're unsure what the standard requires, read the actual grammar/section text (do not rely on memory), or ask before writing the assertion.

This matters because it is easy to write a test that locks in a bug: run the compiler, see what it produced, and assert exactly that. Such a test passes today and keeps passing after a refactor that preserves the bug, but it actively resists ever being fixed -- a correct fix makes a "passing" test start failing, which reads as a regression instead of a correction. This has happened repeatedly in this suite: net-vs-variable modeling was asserted against whatever HLC happened to produce for structs/arrays/typedef'd declarations (should have been asserted per IEEE 1800-2023 Sec 6.7/6.8: no net-type keyword means variable, regardless of `` `default_nettype ``), and attribute attachment was asserted as hoisted onto the module instead of attached to the individual declaration (Sec 5.12). In every case the fix was to rewrite the assertions to match the standard, not to "fix" the standard-based test to match the tool.

**For a known limitation, bug, or missing feature: never assert on what's missing -- that's useless.** Writing `EXPECT_EQ(x, nullptr)` because the field happens to come back empty today just locks the gap in under a different disguise. Instead assert what the standard actually requires, and mark the test as skipped with proper reasoning as the very first line of the test body:
```cpp
GTEST_SKIP() << "HLC does <wrong thing>; should <standard-correct thing> per IEEE 1800-2023 Sec <N>. Fix pending.";
```
so the gap is visible as pending work rather than silently passing against wrong output, or failing and being mistaken for flakiness.

**Except in exceptional cases, don't base tests on what the `.log` dump produces -- avoid reading the log to write tests at all.** A log only captures what the tool *currently* does, which is exactly the same "locks in a bug" trap as trusting tool output directly. Use the header files under `build/include/hldb/` to understand the object model and API surface instead (see Steps 1-3 below). Reading the log is only appropriate for narrow, exceptional confirmations (e.g. the exact wording/location of a diagnostic whose text isn't standard-mandated) -- never as the primary basis for what shape a test should expect.

**When a newly found limitation/bug/missing-feature isn't local to one test file** (a systemic gap likely to recur elsewhere in the suite), document it in the hlc_01 repo's `.claude/instructions/` folder (e.g. `library_discrepancies.md` or a new file) rather than leaving it buried in a single `GTEST_SKIP()` message.

**Never call `getFullName()` / assert on `vpiFullName`.** It's a computed property that is currently wrong in HLC. Use `getName()` only.

**Assert diagnostics that are expected, with `Test::findEvent()`.** When the standard requires HLC to report something (an illegal construct, an unresolvable name), check for that specific event. The `Test` fixture (`hlc/Tests/Test.h`) provides four overloads:
```cpp
const Event *findEvent(uint32_t type);
const Event *findEvent(uint32_t type, std::string_view symbol);
const Event *findEvent(uint32_t type, uint32_t line, uint16_t column);
const Event *findEvent(uint32_t type, std::string_view symbol, uint32_t line, uint16_t column);
```
Prefer the overloads that narrow by `symbol` and/or `line`/`column` over the bare `type`-only one -- confirming the event is about the *specific* named object/location under test is what verifies the right thing was reported, not just that some event of that type exists anywhere in the file. `findEvent()` returns `nullptr` when nothing matches:
```cpp
ASSERT_NE(findEvent(EventType::CompIllegalDefaultPortValue, "port_name"), nullptr);
```
To check every event at one position -- e.g. the copies of one message reached through different inclusions, which differ only in their context notes (`event->getNotes()`) -- use `findEvents(type, line[, column])`. A file that only passes `EventType::X` codes needs `<hlc/Events/EventType.h>`; include `<hlc/Events/EventDefinition.h>` only when it names `Event`, `EventSeverity`, etc.

**Don't write checks that only assert a diagnostic is absent.** `EXPECT_EQ(findEvent(EventType::X), nullptr)` -- "HLC does not report X here" -- is not helpful, so don't add it, one code at a time or as a list of every diagnostic the file could conceivably trigger. Such a check passes when HLC never raises X at all, so it proves nothing about the construct under test, and it inflates the event coverage report with codes no test actually exercises. 107 such checks for 16 codes were removed from this suite (2026-10) for this reason; don't reintroduce them. Show that a legal construct is handled correctly through the model it builds (Steps 1-5 below). If a file must compile without errors, one coarse check covers it:
```cpp
const EventListener::Stats stats = m_session->getEventListener()->getStats();
EXPECT_EQ(stats.m_errors, 0);
```
Beyond that coarse check, don't count errors/warnings: a count breaks the moment reporting improves elsewhere (a previously silent case starts correctly flagging something unrelated to this file), turning a genuine improvement into an apparent regression.

**HLC's diagnostics themselves (`ObjectBinder`'s error reporting, the event definitions in `events/*.yaml`) are a teammate's area -- don't add, extend, or "fix" them unprompted.** Writing a `findEvent()` assertion against an existing event is fine; changing when/whether HLC raises a given event is not your call to make unprompted. If you find a problem in the diagnostics themselves, flag it (as a finding/doc note) rather than fixing it; wait for explicit go-ahead, and check whether it's already being handled first.

**Compare strings as `std::string_view`, never as a bare C-string literal.** Write `EXPECT_EQ(object->getName(), std::string_view("blahblah"))`, not `EXPECT_EQ(object->getName(), "blahblah")`. With a bare literal, a failing expectation can print the literal as a pointer value instead of its text; wrapped in `std::string_view`, gtest prints both strings, so the failure shows what was expected and what came back. The same applies to `ASSERT_EQ`/`EXPECT_NE` and to any other accessor returning text (`getDefName()`, `getValue()`, `getDecompile()`, ...).

**Use named `vpiXXX` constants from the `*vpi_user.h` headers, never magic numbers.** These enumerants are defined in `vpi_user.h` (IEEE-standard constants), `sv_vpi_user.h` (SV extensions), and `hldb_vpi_user.h` (HLDB-specific extensions) -- include whichever one declares the constant you need instead of hardcoding its integer value. Write `EXPECT_EQ(net->getNetType(), vpiWire)`, not `EXPECT_EQ(net->getNetType(), 1)`; write `EXPECT_EQ(op->getOpType(), vpiMinusOp)`, not a raw opcode number -- applies to op types, const types, net types, directions, array types, and every other VPI enumerant. See `hlc/Google/chapter-11/11.7--unsigned_func/test_11.7_unsigned_func.cpp` for a file that consistently does this throughout.

## Overview

Each test targets one SystemVerilog source file under `tests/`. A test consists of two files placed under `hlc/<Group>/<testname>/`:

| File | Purpose |
|---|---|
| `<testname>.hlc` | hlc command line (one line) |
| `test_<testname>.cpp` | GTest fixture + TEST_F cases |

The `.hlc` file and the SV file under `tests/` must already exist before writing a test. The `.hlc` file typically looks like:
```
-wd ../../../tests/<Group> -parse -d db -d ast <testname>.sv -nobuiltin
```

## Step 1 -- Read the SV source and grammar

Read the `.sv` file under `tests/` to understand what SystemVerilog constructs are being tested. Then cross-reference `grammar/SV3_1aParser.g4` to understand the parse structure that HLC will build.

The grammar tells you which rules fire for a given construct -- e.g., `sequence_declaration`, `property_declaration`, `concurrent_assertion_statement` -- and therefore which UHDM objects will be created. Look for the relevant grammar rules and trace how the SV constructs map to UHDM object types.

## Step 2 -- Map grammar constructs to UHDM objects and C++ API

The UHDM object model is generated from YAML files. Headers live in `build/include/hldb/`. **Always read the actual header before using an API -- never guess method names.**

Common construct-to-object mappings:

| SV construct | UHDM object | Header |
|---|---|---|
| `sequence seq_name; ...; endsequence` | `SequenceDecl` | `sequence_decl.h` |
| `property prop_name; ...; endproperty` | `PropertyDecl` | `property_decl.h` |
| `@(posedge clk) expr` | `PropertySpec` (clocking event + expr) | `property_spec.h` |
| `assert property(...)` | `Assert` (inherits `ConcurrentAssertions`) | `assert_stmt.h` |
| `a ##1 b` / `##N expr` | `Operation` with `vpiUnaryCycleDelayOp` | `operation.h` |
| reference to a named decl | `RefObj` | `ref_obj.h` |

UHDM field names map to accessor methods by dropping the `vpi` prefix and lowercasing the first letter:

| UHDM field | C++ method |
|---|---|
| `vpiSequenceDecl` | `getSequenceDecls()` |
| `vpiPropertyDecl` | `getPropertyDecls()` |
| `vpiConcurrentAssertions` | `getConcurrentAssertions()` |
| `vpiProperty` (on Assert) | `getProperty()` |
| `vpiPropertySpec` (on PropertyDecl) | `getPropertySpec()` |
| `vpiClockingEvent` | `getClockingEvent()` |
| `vpiPropertyExpr` | `getPropertyExpr()` |
| `vpiExpr` | `getExpr()` |
| `vpiOperand` (collection) | `getOperands()` |
| `vpiOpType` (scalar) | `getOpType()` |
| `vpiName` | `getName()` |

Collection accessors return `XxxCollection*` (nullptr when absent).
Scalar object accessors return `T*` (nullptr when absent).
All UHDM classes also expose a template getter for direct downcasting:
```cpp
seq->getExpr<hldb::Operation>()          // equivalent to any_cast<hldb::Operation>(seq->getExpr())
found->getProperty<hldb::PropertySpec>() // etc.
```

## Step 3 -- Identify which headers to include

Read the relevant headers in `build/include/hldb/` to confirm method signatures before writing any test code.

Common includes:

```cpp
#include <hldb/Utils.h>              // findByName<T>()
#include <hldb/design.h>             // m_design->getAllModules()
#include <hldb/module.h>             // Module
#include <hldb/sequence_decl.h>      // SequenceDecl  ->  getName(), getExpr()
#include <hldb/property_decl.h>      // PropertyDecl  ->  getName(), getPropertySpec()
#include <hldb/property_spec.h>      // PropertySpec  ->  getClockingEvent(), getPropertyExpr()
#include <hldb/concurrent_assertions.h>  // ConcurrentAssertions  ->  getProperty()
#include <hldb/assert_stmt.h>        // Assert (inherits ConcurrentAssertions)
#include <hldb/operation.h>          // Operation  ->  getOpType(), getOperands()
#include <hldb/ref_obj.h>            // RefObj  ->  getName(), getActual()
```

Key constants in `build/include/hldb/sv_vpi_user.h`:
- `vpiUnaryCycleDelayOp` = 53 -- `##N expr` or `expr ##N expr` (unary/binary cycle delay)
- `vpiCycleDelayOp`      = 54 -- binary cycle delay
- `vpiPosedge`           = 39 -- posedge event (opType on clocking Operation)

## Step 4 -- Write the fixture

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

## Step 5 -- Write the test cases

Typical progression per feature:

1. **Existence** -- the module/decl/assertion is present and non-null.
2. **Content** -- the key field (expression, clocking event, property expr) is non-null.
3. **Shape** -- opType, operand count, or referenced name matches what the grammar dictates.
4. **Absence** -- confirm that the model has no object the standard rules out (e.g., no `PropertyDecl` when the assert is inline). This is about model shape, not diagnostics: don't assert that a diagnostic is absent (see the core principles above).

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

## Avoiding filename collisions

CMake globs all `hlc/**/test_*.cpp` files and builds one executable per file, with output flattened by basename -- two test files with the same name in different folders collide at CMake configure time (duplicate target name). Before naming a new test file, check whether the basename already exists elsewhere in the tree. If a collision would occur (e.g. `ansi/module.sv` and `nonansi/module.sv` would both naturally want `test_5.1.2-module.cpp`), prefix the filename with the parent folder's name to disambiguate: `test_5.1.2-ansi_module.cpp` and `test_5.1.2-nonansi_module.cpp`.

## Templated getXX<T>() pitfall

Be careful with templated accessors when checking for absence. `ASSERT_EQ(ptr->getActual<Net>(), nullptr)` is true both when `getActual()` is genuinely null AND when it's non-null but simply isn't a `Net` -- these are two different facts, and the templated form conflates them. Avoid the templated overload for a null check; use the plain (non-templated) accessor instead. If you need to assert both "present" and "is specifically type T," split it into two assertions:
```cpp
ASSERT_NE(ptr->getActual(), nullptr);
EXPECT_EQ(ptr->getActual()->getAnyType(), hldb::AnyType::Net);
```
rather than relying on a single templated-downcast comparison to carry both meanings.

## Look beyond existence and name

Depending on the construct under test, also check properties like `getScalared()`/`getVectored()`, `getNetType()`, `getIsMethodCall()`, signed/unsigned, packed/unpacked, and similar flags. A test that only confirms a node exists and is named correctly is leaving real coverage on the table.

## Inheritance to keep in mind

| Class | Parent | Notable extras |
|---|---|---|
| `Module` | `Instance` -> `Scope` -> `Any` | `getSequenceDecls()`, `getPropertyDecls()`, `getConcurrentAssertions()` are on `Scope` |
| `Assert` | `ConcurrentAssertions` -> `Any` | `getProperty()` (-> `PropertySpec`) is on `ConcurrentAssertions` |
| `Operation` | `Expr` -> `SimpleExpr` -> `Any` | `getOpType()`, `getOperands()` |
| `RefObj` | `SimpleExpr` -> `Any` | `getName()`, `getActual()` |

## Coding conventions

- **ASCII only.** All code and comments you add or modify must use plain ASCII
  characters (0x00-0x7F) only. Do not introduce any non-ASCII / Unicode
  characters - no smart quotes (use `'` and `"`), no em/en dashes (use `-` or
  `--`), no arrows (write `->`), no ellipsis character (write `...`), no
  non-breaking spaces, and no Unicode box-drawing or symbol characters in
  comments. This applies to source, headers, tests, and inline comments.

- **Container insertion.** Prefer `emplace` / `emplace_back` over
  `insert` / `push_back` when adding elements to standard containers.

- **Explicit types.** Prefer explicit type names over `auto`. Only use `auto`
  when the type is genuinely verbose or already stated in the same expression
  (e.g. the result of a cast or `make_shared`).

- **Member naming.** Prefix all struct and class data members with `m_`
  (e.g. `m_fileId`, `m_startLine`).

- **clang-format.** All authored or modified C++ code must conform to the
  project `.clang-format` in the repository root. Run `clang-format` on any
  `.cpp` or `.h` file you add or change before committing.

- **Test class naming.** Test class names must be suffixed with `Test`
  (e.g. `class InterfaceIdentifiersTest : public Test`).

## License header

Every new `.cpp` file must begin with the Apache 2.0 header attributed to **Apotell** (see `CLAUDE.md`).
