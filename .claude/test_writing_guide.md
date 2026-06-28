# davtests — Test Writing Guide

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

## Step 1 — Read the SV source and grammar

Read the `.sv` file under `tests/` to understand what SystemVerilog constructs are being tested. Then cross-reference `grammar/SV3_1aParser.g4` to understand the parse structure that HLC will build.

The grammar tells you which rules fire for a given construct — e.g., `sequence_declaration`, `property_declaration`, `concurrent_assertion_statement` — and therefore which UHDM objects will be created. Look for the relevant grammar rules and trace how the SV constructs map to UHDM object types.

## Step 2 — Map grammar constructs to UHDM objects and C++ API

The UHDM object model is generated from YAML files. Headers live in `build/include/hldb/`. **Always read the actual header before using an API — never guess method names.**

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

## Step 3 — Identify which headers to include

Read the relevant headers in `build/include/hldb/` to confirm method signatures before writing any test code.

Common includes:

```cpp
#include <hldb/Utils.h>              // findByName<T>()
#include <hldb/design.h>             // m_design->getAllModules()
#include <hldb/module.h>             // Module
#include <hldb/sequence_decl.h>      // SequenceDecl  →  getName(), getExpr()
#include <hldb/property_decl.h>      // PropertyDecl  →  getName(), getPropertySpec()
#include <hldb/property_spec.h>      // PropertySpec  →  getClockingEvent(), getPropertyExpr()
#include <hldb/concurrent_assertions.h>  // ConcurrentAssertions  →  getProperty()
#include <hldb/assert_stmt.h>        // Assert (inherits ConcurrentAssertions)
#include <hldb/operation.h>          // Operation  →  getOpType(), getOperands()
#include <hldb/ref_obj.h>            // RefObj  →  getName(), getActual()
```

Key constants in `build/include/hldb/sv_vpi_user.h`:
- `vpiUnaryCycleDelayOp` = 53 — `##N expr` or `expr ##N expr` (unary/binary cycle delay)
- `vpiCycleDelayOp`      = 54 — binary cycle delay
- `vpiPosedge`           = 39 — posedge event (opType on clocking Operation)

## Step 4 — Write the fixture

Follow this exact skeleton (copy from an existing test):

```cpp
class MyTest : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "mytestname.hlc"});
    ASSERT_NE(m_session,  nullptr);
    ASSERT_NE(m_compiler, nullptr);
    ASSERT_NE(m_design,   nullptr);
  }

  static void TearDownTestSuite() {
    m_design = nullptr;
    delete m_compiler;  m_compiler = nullptr;
    delete m_session;   m_session  = nullptr;
  }
};
```

`m_session`, `m_compiler`, and `m_design` are static members inherited from `Test`.

## Step 5 — Write the test cases

Typical progression per feature:

1. **Existence** — the module/decl/assertion is present and non-null.
2. **Content** — the key field (expression, clocking event, property expr) is non-null.
3. **Shape** — opType, operand count, or referenced name matches what the grammar dictates.
4. **Absence** — confirm that something is NOT present (e.g., no `PropertyDecl` when the assert is inline).

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

## Inheritance to keep in mind

| Class | Parent | Notable extras |
|---|---|---|
| `Module` | `Instance` → `Scope` → `Any` | `getSequenceDecls()`, `getPropertyDecls()`, `getConcurrentAssertions()` are on `Scope` |
| `Assert` | `ConcurrentAssertions` → `Any` | `getProperty()` (→ `PropertySpec`) is on `ConcurrentAssertions` |
| `Operation` | `Expr` → `SimpleExpr` → `Any` | `getOpType()`, `getOperands()` |
| `RefObj` | `SimpleExpr` → `Any` | `getName()`, `getActual()` |

## License header

Every new `.cpp` file must begin with the Apache 2.0 header attributed to **Apotell** (see `CLAUDE.md`).
