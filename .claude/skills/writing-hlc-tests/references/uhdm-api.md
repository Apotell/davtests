# UHDM object model and C++ API reference

The UHDM object model is generated from YAML files. Headers live in
`build/include/hldb/`. **Always read the actual header before using an API -- never
guess method names.** The tables below are a starting map, not a substitute for the
header.

## Construct-to-object mappings

| SV construct | UHDM object | Header |
|---|---|---|
| `sequence seq_name; ...; endsequence` | `SequenceDecl` | `sequence_decl.h` |
| `property prop_name; ...; endproperty` | `PropertyDecl` | `property_decl.h` |
| `@(posedge clk) expr` | `PropertySpec` (clocking event + expr) | `property_spec.h` |
| `assert property(...)` | `Assert` (inherits `ConcurrentAssertions`) | `assert_stmt.h` |
| `a ##1 b` / `##N expr` | `Operation` with `vpiUnaryCycleDelayOp` | `operation.h` |
| reference to a named decl | `RefObj` | `ref_obj.h` |

## Field-to-method naming rule

UHDM field names map to accessor methods by dropping the `vpi` prefix and lowercasing
the first letter:

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

Note the null-check pitfall with these templated getters -- see SKILL.md.

## Common includes

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

## Key constants

VPI enumerants come from `vpi_user.h` (IEEE-standard), `sv_vpi_user.h` (SV
extensions), and `hldb_vpi_user.h` (HLDB-specific). Always use the named constant,
never the integer value.

In `build/include/hldb/sv_vpi_user.h`:

- `vpiUnaryCycleDelayOp` = 53 -- `##N expr` or `expr ##N expr` (unary/binary cycle delay)
- `vpiCycleDelayOp`      = 54 -- binary cycle delay
- `vpiPosedge`           = 39 -- posedge event (opType on clocking Operation)

## Inheritance to keep in mind

| Class | Parent | Notable extras |
|---|---|---|
| `Module` | `Instance` -> `Scope` -> `Any` | `getSequenceDecls()`, `getPropertyDecls()`, `getConcurrentAssertions()` are on `Scope` |
| `Assert` | `ConcurrentAssertions` -> `Any` | `getProperty()` (-> `PropertySpec`) is on `ConcurrentAssertions` |
| `Operation` | `Expr` -> `SimpleExpr` -> `Any` | `getOpType()`, `getOperands()` |
| `RefObj` | `SimpleExpr` -> `Any` | `getName()`, `getActual()` |
