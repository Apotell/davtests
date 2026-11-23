# HLDB -- What davtests Uses

HLDB is the object model HLC builds: one C++ class per IEEE 1800-2023 object (Ch. 37, VPI object
model), owned by a database, with typed accessors, a VPI layer, serialization and tools. davtests
uses only what the HLDB installation provides; this file describes that and nothing else.

**Rule:** refer only to installed HLDB headers, classes and accessors. Never name HLDB's model
sources, generators, templates or private implementation functions -- they are not public.

## Installation layout

| Path | Content |
|---|---|
| `include/hldb/` | Every model class header, snake_case (`module.h`, `ref_obj.h`, `class_typespec.h`, ...), plus the headers below |
| `include/hldb/hldb.h` | Master include |
| `include/hldb/vpi_user.h`, `sv_vpi_user.h`, `hldb_vpi_user.h` | VPI constants (IEEE, SV extensions, HLDB extensions) |
| `share/hldb/HLDB.capnp` | Serialization schema of a saved design (`design.hldb`) |
| `share/hldb/events.json` | HLDB's own events (codes, messages) |
| `bin/hldb-dump`, `hldb-cmp`, `hldb-equiv`, `hldb-lint`, `hldb-gc`, `hldb-reduce` | Tools over a saved design |
| `lib/cmake/HLDB/` | `HLDBConfig.cmake` |

## Headers davtests uses

| Header | What it gives |
|---|---|
| `<class_name>.h` | The class: typed accessors (`getName()`, `getActual()`, `getParamAssigns()`, ...) |
| `any.h` | `hldb::Any`, the base of every object: `getAnyType()`, `getParent()`, location (`getFile()`, `getStartLine()`, ...) |
| `RTTI.h` | `any_cast<T>()`, `isa<T>()` |
| `Utils.h` | `findByName<T>()`, `findByDefName<T>()`, `getActual<T>()`, `getTypespec<T>()`, `getDefName()`, `getNets()`/`getPorts()`/`getVariables()`/`getParameters()`, `getLexicalParent()`, `getCompilationUnit()`, ... |
| `ExprEval.h`, `Reducer.h` | Expression evaluation and reduction |
| `Finder.h` | Hierarchical lookup |
| `Pruner.h`, `GarbageCollector.h` | Typespec merging, removal of unreachable objects |
| `Serializer.h` | `make<T>()`, `save()`, `restore()` |
| `Visitor.h`, `Listener.h` | Graph traversal |
| `Linter.h` | Model checks; each class's `isFiltered()` tells whether an object is allowed in a relation |

## Model notes

- Objects are owned by the database/serializer; pointers are raw, there is no reference counting.
- A reference (`RefObj`, `RefTypespec`) points at what it names through `getActual()`; follow it
  with `hldb::getActual<T>()`.
- A class's type is `getAnyType()` (`hldb::AnyType::Module`, ...); use it to check a type.
- Use the typed accessors, never the VPI layer (`getVpiPropertyValue()`, `getByVpiType()`), except
  in tests of the VPI API itself. VPI constants are the right way to spell expected values
  (`EXPECT_EQ(port->getDirection(), vpiInput)`).
- Use `getName()`, never `getFullName()`/`vpiFullName`. Definitions, instances and
  module/interface/program/class typespecs carry a decorated name (`C#(.W(8))`); look them up
  with `findByDefName<T>()` and compare `getDefName()`.
