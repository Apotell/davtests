# HLDB -- Installed Headers Reference

Everything below is installed under `include/hldb/` and is part of HLDB's public API. Nothing
else of HLDB is public; do not refer to its sources, model files, generators or templates.

## Foundations

| Header | Role |
|---|---|
| `any.h` | `hldb::Any`, base of every object: type (`getAnyType()`), parent, location |
| `RTTI.h` | `any_cast<T>()`, `isa<T>()` |
| `vpi_user.h` | IEEE VPI constants and C API |
| `sv_vpi_user.h` | SystemVerilog VPI extensions |
| `hldb_vpi_user.h` | HLDB-specific VPI extensions |
| `SymbolFactory.h`, `SymbolId.h` | Interned strings, `BadSymbolId` |
| `Event.h`, `EventDefinition.h`, `EventListener.h` | Events and their listener |

## Model

| Header | Role |
|---|---|
| `hldb.h` | Includes every model class |
| `<class_name>.h` | One per model class, snake_case (`module.h`, `net.h`, `ref_obj.h`, `class_defn.h`, ...): typed accessors |
| `hldb_types.h` | `hldb::AnyType` |
| `hldb_forward_decl.h`, `containers.h` | Forward declarations, collection types (`VectorOf...`, `AnyCollection`) |

## Utilities and services

| Header | Role |
|---|---|
| `Utils.h` | Lookup and navigation helpers (`findByName`, `findByDefName`, `getActual`, `getTypespec`, `getDefName`, `getParameters`, `getLexicalParent`, ...) -- check here before writing a helper |
| `Serializer.h` | Object factory, save/restore |
| `Visitor.h`, `Listener.h`, `VpiListener.h` | Traversal |
| `Linter.h` | Model checks, `isFiltered()` |
| `ExprEval.h`, `Reducer.h`, `NumUtils.h` | Expression evaluation, reduction, numeric helpers |
| `Finder.h` | Hierarchical lookup |
| `Pruner.h`, `GarbageCollector.h` | Typespec merging, garbage collection |
| `Cloner.h`, `Comparer.h`, `Printer.h` | Cloning, comparison, printing of objects |
