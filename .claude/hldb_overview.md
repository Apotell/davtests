# hldb — UHDM Code Generator Overview

**hldb** is the UHDM (Universal Hardware Data Model) project — a **code generator**, not a hand-written library. It lives at `third_party/hldb/` as a git submodule.

**Why it exists:** Provides a canonical IEEE SystemVerilog object model that hlc populates during elaboration. Tools like Yosys, Verilator, linters, and formal tools consume the UHDM graph via VPI or C++ API.

**Key rule:** Any change to the object model starts in `model/*.yaml`. Never hand-edit `src/` or generated headers — CMake runs `generate.py` automatically before compilation.

## What it generates

From ~160 YAML model files:
- C++ class headers + implementations (one per SV object type)
- VPI facades (C-level IEEE interface)
- Cap'n Proto serialization/deserialization
- Visitor/Listener traversal patterns
- Linter with `isFiltered()` type validation
- Python bindings (pybind11 → `pyhldb`)

## Directory structure

| Dir | Role |
|---|---|
| `model/` | ~160 YAML files — source of truth for all object types |
| `scripts/` | Python generator modules; `generate.py` is the dispatcher (8 parallel workers) |
| `templates/` | Hand-written C++ skeletons that generators fill in |
| `include/hldb/` | Generated headers + hand-written foundations (`any.h`, `RTTI.h`, `vpi_user.h`) |
| `src/` | Generated C++ implementations (never edit) |
| `python/` | pybind11 wrappers (mostly generated) |
| `tests/` | C++ unit tests + Python pytest tests |

## Code generation pipeline

```
model/*.yaml → scripts/loader.py (parse + validate) → scripts/classes.py + 28 other generators
             → include/hldb/*.h + src/*.cpp + python/py_*.cpp
```

Key scripts:
- `scripts/loader.py` — YAML parser; validates all cross-references (`group_ref` → `group_def`, etc.)
- `scripts/config.py` — naming conventions: `make_class_name()`, `make_func_name()`, `make_var_name()`
- `scripts/classes.py` — generates bulk of C++ class definitions, accessors, `isFiltered()` stubs
- `scripts/Linter.py` — generates `Linter.h/cpp` that walks the object graph calling `isFiltered()`
- `scripts/generate.py` — dispatcher; runs workers in parallel

All scripts use **2-space indentation**.

## YAML model concepts

| Key | Meaning |
|---|---|
| `obj_def` | Concrete (leaf) allocatable SV object (e.g., `module`, `net`, `variable`) |
| `class_def` | Abstract/virtual class for inheritance (e.g., `instance`, `scope`, `expr`) |
| `property` | Scalar field (string/int/bool); accessed via `vpi_get()` |
| `obj_ref` | Strongly-typed reference to a specific obj_def; `card: 1` or `card: any` |
| `class_ref` | Polymorphic reference to any subclass of a class_def |
| `group_def` | Named union of allowed types (filter list using `includes:` / `excludes:`) |
| `group_ref` | Polymorphic field typed to a group_def; generates `isFiltered()` + `Any*` member |
| `extends` | Inheritance: `module extends instance extends scope` |

Example from `variable.yaml`:
```yaml
- group_def: drivers_group
  includes: [assign_stmt, cont_assign, force, ports]
- group_ref: driver
  vpi: vpiDriver
  type: drivers_group
  card: any
```
→ Generates `AnyCollection* m_drivers`, getter, and `isFiltered()` checking `isa<AssignStmt>() || ...`

## The Serializer (factory + owner)

```cpp
Serializer s;
Module *m = s.make<Module>();   // allocate; Serializer owns lifetime
s.save("design.hldb");          // Cap'n Proto binary save
s.restore("design.hldb");       // reload
```
All objects are owned by `Serializer`; delete only via `erase()` or `purge()`. No reference counting — pointers are raw.

## The Linter

- `isFiltered(const Any* data, int32_t relation)` — base in `any.h` returns `true` (permissive default)
- Generated override in each class: validates polymorphic `group_ref` fields against their filter
- `Linter.cpp` walks the full object graph calling `isFiltered()` and reporting violations via `EventListener`

## VPI layer

Generated per `obj_def`: `getVpiType()`, `getByVpiName()`, `getByVpiType()`, `getVpiPropertyValue()`. C-level functions (`vpi_get`, `vpi_handle`, `vpi_iterate`, `vpi_scan`) delegate to these methods.

## Tests

- C++: `ctest --test-dir out/build -C Release --output-on-failure`
- Python: `pytest tests/ -v` (requires `pyhldb` installed: `pip install -e .`)

See [hldb_key_files.md](hldb_key_files.md) for a breakdown of hand-written vs. generated files.
