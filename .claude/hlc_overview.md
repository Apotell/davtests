# Hlc — Architecture Overview

Hlc is a full SystemVerilog 2017 front-end: preprocessor → ANTLR4 parser → two-phase elaborator → UHDM (Universal Hardware Data Model) output. It is used as a front-end by simulators, synthesizers, linters, and formal verification tools.

**Goal:** Produce a correct, fully elaborated UHDM design graph that 3rd-party tools can consume via VPI without re-parsing SystemVerilog.

## Pipeline

```
SV sources → CommandLineParser → PreprocessFile (PPCache) → ParseFile/ANTLR4 (ParseCache)
           → AST VObject tree → Phase1ModelBuilder (create UHDM objects)
           → Phase2ModelBuilder (add detail) → ObjectBinder (resolve symbols)
           → IntegrityChecker → UHDM design graph (on-disk / VPI API)
```

## Key data structures

| Type | File | Role |
|---|---|---|
| `VObject` | `include/Hlc/Design/VObject.h` | AST node: name (SymbolId), file (PathId), type (VObjectType), parent/child/sibling tree |
| `FileContent` | `include/Hlc/Design/FileContent.h` | Container of VObjects for one source file; provides `sl_collect*` search |
| `DesignElement` | `include/Hlc/Design/DesignElement.h` | Module/package/class declaration with Push/Pop scope events |
| `PathId` | `include/Hlc/Common/PathId.h` | Abstract file path ID (uint32_t into SymbolTable); all ops via FileSystem |
| `SymbolId` | `include/Hlc/Common/SymbolId.h` | Interned string ID (uint32_t into SymbolTable) |
| `NodeId` | `include/Hlc/Common/NodeId.h` | Unique ID for a VObject within a FileContent or elaborated design |
| `Session` | `include/Hlc/Common/Session.h` | Context facade: FileSystem, SymbolTable, ErrorContainer, CommandLineParser |
| `CompilationUnit` | `include/Hlc/SourceCompile/CompilationUnit.h` | Groups source files; tracks macros, timescales, node ID generator |
| `Compiler` | `include/Hlc/SourceCompile/Compiler.h` | Top-level orchestrator; runs full pipeline |
| `instance_node_map_t` | `include/Hlc/Common/Containers.h` | `map<(PathId,NodeId), hldb::Any*>` — bridges AST nodes to UHDM objects |

## Top-level directories

- `src/` + `include/Hlc/` — implementation and public API (mirror structure)
- `grammar/` — ANTLR4 `.g4` grammars (SV3_1a lexer/parser, preprocessor, splitter)
- `third_party/hldb/` — UHDM subproject (code generator for IEEE SV object model)
- `third_party/antlr4/` — ANTLR4 C++ runtime
- `tests/` — regression test directories (each: `dut.sv`, `*.sl` spec, optional `*.json`)
- `src/Cache/` — Cap'n Proto-based PPCache and ParseCache

## Build

CMake 3.20+. Key generated targets: `GenerateParser`, `GenerateParserListeners`, `GenerateCacheSerializers`. Generated code lands in `${GENDIR}/`.

**Never hand-edit** files under `src/SourceCompile/SV3_1a*TreeListener*` or `include/Hlc/SourceCompile/AstListener.h` — they are regenerated at build time from the grammar.
