# hldb — Key Files Reference

## Hand-written foundations (safe to edit)

| File | Role |
|---|---|
| `include/uhdm/any.h` | Base `Any` class — default `isFiltered()` (returns true), RTTI base |
| `include/uhdm/RTTI.h` | Custom RTTI system (not `std::typeinfo`) — `isa<T>()`, `cast<T>()` |
| `include/uhdm/vpi_user.h` | IEEE VPI standard C header |
| `include/uhdm/sv_vpi_user.h` | SV extensions to VPI |
| `include/uhdm/uhdm_vpi_user.h` | UHDM-specific VPI extensions |
| `include/uhdm/SymbolFactory.h` | String intern pool (extended by hlc's SymbolTable) |
| `include/uhdm/SymbolId.h` | Symbol ID type + `BadSymbolId` sentinel |
| `include/uhdm/UhdmFinder.h` | Hierarchical finder utilities |
| `include/uhdm/EventListener.h` | Event callback interface used by Linter |
| `templates/Serializer.h` | Factory, save/restore, object ownership |
| `templates/Linter.cpp` | Hand-written lint hook stubs: `checkXxx_()` private methods |

## Generated files (never hand-edit)

| Pattern | Generator | Content |
|---|---|---|
| `include/uhdm/<ClassName>.h` | `classes.py` | Data members, accessors, `isFiltered()` declaration |
| `src/<ClassName>.cpp` | `classes.py` | Accessor implementations |
| `include/uhdm/Linter.h` | `Linter.py` | `checkXxx()` declarations |
| `src/Linter.cpp` | `Linter.py` | Walk + `isFiltered()` implementations |
| `include/uhdm/uhdm.h` | aggregate | Master include |
| `include/uhdm/uhdm_types.h` | | Type enums |
| `include/uhdm/uhdm_forward_decl.h` | | Forward declarations |
| `include/uhdm/Serializer.h` | `serializer.py` | `make<T>()` factory |
| `src/Serializer.cpp` | `serializer.py` | Save/restore logic |
| `include/uhdm/UhdmListener.h` + `.cpp` | `UhdmListener.py` | `enter`/`leave` traversal |
| `include/uhdm/UhdmVisitor.h` + `.cpp` | `UhdmVisitor.py` | `visit` traversal |
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
