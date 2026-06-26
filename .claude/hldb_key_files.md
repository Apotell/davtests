# hldb — Key Files Reference

## Hand-written foundations (safe to edit)

| File | Role |
|---|---|
| `include/hldb/any.h` | Base `Any` class — default `isFiltered()` (returns true), RTTI base |
| `include/hldb/RTTI.h` | Custom RTTI system (not `std::typeinfo`) — `isa<T>()`, `cast<T>()` |
| `include/hldb/vpi_user.h` | IEEE VPI standard C header |
| `include/hldb/sv_vpi_user.h` | SV extensions to VPI |
| `include/hldb/hldb_vpi_user.h` | UHDM-specific VPI extensions |
| `include/hldb/SymbolFactory.h` | String intern pool (extended by hlc's SymbolTable) |
| `include/hldb/SymbolId.h` | Symbol ID type + `BadSymbolId` sentinel |
| `include/hldb/Finder.h` | Hierarchical finder utilities |
| `include/hldb/EventListener.h` | Event callback interface used by Linter |
| `templates/Serializer.h` | Factory, save/restore, object ownership |
| `templates/Linter.cpp` | Hand-written lint hook stubs: `checkXxx_()` private methods |

## Generated files (never hand-edit)

| Pattern | Generator | Content |
|---|---|---|
| `include/hldb/<ClassName>.h` | `classes.py` | Data members, accessors, `isFiltered()` declaration |
| `src/<ClassName>.cpp` | `classes.py` | Accessor implementations |
| `include/hldb/Linter.h` | `Linter.py` | `checkXxx()` declarations |
| `src/Linter.cpp` | `Linter.py` | Walk + `isFiltered()` implementations |
| `include/hldb/hldb.h` | aggregate | Master include |
| `include/hldb/any_types.h` | | Type enums |
| `include/hldb/forward_decl.h` | | Forward declarations |
| `include/hldb/Serializer.h` | `serializer.py` | `make<T>()` factory |
| `src/Serializer.cpp` | `serializer.py` | Save/restore logic |
| `include/hldb/Listener.h` + `.cpp` | `Listener.py` | `enter`/`leave` traversal |
| `include/hldb/Visitor.h` + `.cpp` | `Visitor.py` | `visit` traversal |
| `python/py_*.cpp` | `py_classes.py` etc. | pybind11 wrappers |

## Key model YAML files (source of truth)

| File | Defines |
|---|---|
| `model/instance.yaml` | `instance` class_def — base for all instantiations |
| `model/module_inst.yaml` | `module_inst` obj_def — elaborated module instance |
| `model/module.yaml` | `module` obj_def — module definition |
| `model/scope.yaml` | `scope` class_def — base for scoped constructs |
| `model/ref_typespec.yaml` | `ref_typespec` — reference to a type |
| `model/class_typespec.yaml` | `class_typespec` — class type specification |
| `model/variable.yaml` | `variable` class_def — variables with drivers/loads (good group_ref example) |
