# HLC -- What davtests Uses

HLC is a SystemVerilog (IEEE 1800-2023) front-end. It preprocesses and parses the sources and
builds an HLDB design graph (see [hldb_overview.md](hldb_overview.md)) that other tools consume
through the C++ API or VPI, without re-parsing SystemVerilog. davtests uses only what the HLC
installation provides; this file describes that and nothing else.

**Rule:** tests, comments and test messages refer only to the installed API below, to the HLDB
model and to the IEEE standard. Never name HLC's internal classes, passes, functions or source
files -- they are not public. Describe what HLC does in terms of its observable result instead
("HLC resolves the name to the enclosing class's typedef"), not how it does it.

## Installation layout

| Path | Content |
|---|---|
| `bin/hlc` | The compiler executable (command-line options as in its `-help`) |
| `bin/hlc.conf` | Logging/cache configuration |
| `lib/` | `hlc` (and `hlc_test`) libraries, with the libraries they need |
| `lib/cmake/HLC/` | `HLCConfig.cmake` for `find_package(HLC)`; target `hlc::hlc` |
| `include/hlc/hlc.h`, `config.h`, `version.h` | Top-level include, build configuration, version |
| `include/hlc/Common/` | `Session`, `FileSystem`, `PathId`, `SymbolId`, `NodeId`, `Logger`, `Profile`, `RTTI`, `HLCConfig` |
| `include/hlc/Events/` | `EventType.h` (event codes), `EventDefinition.h`, `EventListener.h`, `EventContext.h`, `Waiver.h` |
| `include/hlc/SourceCompile/` | `Compiler`, `CompileAction`, `AstNode`, `AstListener` (one `enter*`/`leave*` callback per grammar rule), `VObjectTypes.h` (AST node types) |
| `include/hlc/Tests/Test.h` | The gtest fixture davtests derives from |
| `share/hlc/events.json` | Every HLC event: code, name, severity, parameters, message |

## Using it from a test

- Derive from the `Test` fixture and call `Compile(__FILE__, {"-f", "<name>.hlc"})` in
  `SetUpTestSuite()`. The fixture owns the `Compiler` and its `Session`; the compiled design is
  `hldb::Design`, reached from the compiler.
- Check diagnostics with `findEvent(EventType::X, ...)` / `findEvents(...)`; event codes come from
  `hlc/Events/EventType.h`.
- The fixture also reports which design elements the compiled design reaches from its top-level
  modules and programs (IEEE 1800-2023 Sec 23.3.1, 24.3).
- Navigate the model with HLDB's typed accessors and `hldb/Utils.h` (see
  [hldb_overview.md](hldb_overview.md)).

## What HLC produces (observable behavior)

- A non-elaborated design: definitions (modules, interfaces, programs, packages, classes, UDPs,
  checkers) with their contents, compilation units, source files, and instantiations recorded as
  written. Names are bound to their declarations (`RefObj::getActual()`,
  `RefTypespec::getActual()`).
- Parameterized definitions used with overrides get specialized definitions
  (`getIsSpecialization()`), one per distinct set of parameter values.
- Diagnostics are reported through the `EventListener`. A diagnostic inside a parameterized
  definition the design never uses is not reported; anything else is.
