# NetsAndVariables -- required diagnostics audit

Fixture directory: `tests\NetsAndVariables` (53 files, all `.sv`)
Derived from IEEE Std 1800-2023 (`third_party\System-Verilog-1800-2023.pdf`) read per
construct; `sv_error_catalog.md` used only as a cross-check afterwards.
HLC behavior measured with `out\build\bin\hlc.exe` built from `master` @ `923a6d461`
(rebuilt 2026-08-12) as `hlc -wd <dir> -nobuiltin <file>`.

Codes marked ADDED exist in `ErrorDefinition.h/.cpp` but are **NOT raised anywhere yet**
-- see Handoff.

| Verdict | Count | Meaning |
|---|---|---|
| LEGAL | 30 | standard permits it; HLC accepts it |
| EXISTS | 12 | violation, and HLC already diagnoses it |
| EXISTS-UNWIRED | 3 | violation; a fitting code exists but no call site fires |
| ADDED | 4 | violation; new code minted here (2 codes, 4 files) |
| FALSE-POSITIVE | 4 | legal code that HLC wrongly rejects |

`FALSE-POSITIVE` is a verdict the sv-error-register skill does not define; this run
needed it (see HLC bugs). The skill should gain it.

## Reading the Grammar production column

Every row whose violation is **grammatical** names the Annex A production that cannot
derive the construct -- that is the evidence the rule is a parse error rather than a
semantic one. `(satisfied)` marks a LEGAL row whose legality was decided *by* a
production rather than by prose. `-` means no production is the deciding factor (the rule
is prose-only, or the row is a false positive / model bug).

| Production | Annex A subclause | Used by |
|---|---|---|
| `module_common_item` | A.1.4 Module items | contrast evidence: it **does** list `always_construct` and `program_instantiation` |
| `non_port_program_item` | A.1.7 Program items | program contents |
| `checker_or_generate_item_declaration` | A.1.8 Checker items | `[ rand ] data_declaration` |
| `class_property` | A.1.9 Class items | the only home of `random_qualifier` |
| `package_item` / `package_or_generate_item_declaration` | A.1.11 Package items | package contents |
| `data_declaration` | A.2.1.3 Type declarations | `[const] [var] [lifetime] data_type_or_implicit ...` |
| `var_data_type` | A.2.2.1 Net and variable types | `data_type \| var data_type_or_implicit` |
| `assertion_variable_declaration` | A.2.10 Assertion declarations | property/sequence local variables |
| `default_nettype_value` | Syntax 22-7 (22.8) -- the standard states this one is **not in Annex A** | the directive's argument |

Two practical notes on the parse-error rows, measured rather than assumed: one violation
yields several diagnostics (`ProgramAlways.sv` -> 3, `BadArgumentType.sv` -> 7), so tests
must use `findError()` and never an error count; and the model still gets built after
recovery (`ProgramAlways.sv` still serializes its `Program` plus `prog_var`, dropping only
the `always` block), so a test may assert the syntax error *and* inspect the partial model.

---

## (root)

| File | Line | Construct / trigger | Clause | Grammar production | Normative wording | Catalog row | Verdict | Error code | Severity | Pass | Payload | Notes |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| dut.sv | - | ANSI + non-ANSI port kind permutations, all 12 net_type keywords, all variable data types, `var` forms | 23.2.2.3, 6.7, 6.8 | - | "For output ports ... If the data type is declared with the explicit data_type syntax, the port kind shall default to variable" | - | LEGAL | - | - | - | - | Verified the file's own port-kind comments against 23.2.2.3: `input logic` -> net, `output logic` -> variable, both correct. Compiles with 0 errors |

## Ansi

| File | Line | Construct / trigger | Clause | Grammar production | Normative wording | Catalog row | Verdict | Error code | Severity | Pass | Payload | Notes |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Assertion.sv | - | `int` local variable in a property | 16.10 | `assertion_variable_declaration` (A.2.10) (satisfied) | `assertion_variable_declaration ::= var_data_type list_of_variable_decl_assignments ;` | - | LEGAL | - | - | - | - | `int` is a data_type, so it satisfies var_data_type |
| Behavior.sv | - | nets + variables, implicit nets by continuous assign, all four `always` forms | 6.10, 6.5 | - | "If an identifier appears on the left-hand side of a continuous assignment statement ... an implicit scalar net of default net type shall be assumed" | - | LEGAL | - | - | - | - | Implicit nets `implicit_net_a`/`_b` are 6.10 bullet 3; each variable has exactly one driver kind |
| Checker.sv | 21 | `rand bit [3:0] ck_rand;` in a checker | 17.2 | `checker_or_generate_item_declaration` (A.1.8) (satisfied) | `checker_or_generate_item_declaration ::= [ rand ] data_declaration \| ...` | - | LEGAL | - | - | - | - | `rand` is explicitly permitted on a checker data declaration |
| Checker.sv | 22 | `randc bit [1:0] ck_randc;` in a checker | 17.2 | **`checker_or_generate_item_declaration` (A.1.8) -- admits `[ rand ]` only** | the production has no `randc` alternative; `randc` appears nowhere in Clause 17 | - | EXISTS | PA_SYNTAX_ERROR | SYNTAX | PARSE | - | UNCLAIMED by the fixture, which labels it "checker randc variable declaration". Confirmed illegal: `randc` does not occur anywhere in pages 503-522. HLC already rejects it (`[SNT:PA0207] Checker.sv:22:2 extraneous input 'randc'`) -- so **the fixture is wrong, not HLC**. See Fixture corrections |
| Class.sv | - | `rand`/`randc` class properties | 18.4 | `class_property` (A.1.9) (satisfied) | "Class variables can be declared random using the rand and randc modifier keywords" | 600 | LEGAL | - | - | - | - | `random_qualifier` is legal on `class_property`; this is the one scope where `randc` belongs |
| Implicit.sv | - | implicit nets via continuous assignment | 6.10 | - | as Behavior.sv | - | LEGAL | - | - | - | - | Also carries a correct trailing comment that implicit declarations are net-only |
| Interface.sv | 22 | `modport mp(input if_logic, output if_wire)` where `if_wire` is a declared `wire` | 25.5 | - | modport list items may name nets or variables of the interface | - | FALSE-POSITIVE | COMP_MODPORT_UNDEFINED_PORT | ERROR | COMP | - | `wire if_wire;` is declared on line 21; HLC reports `[ERR:CP0311] Undefined net used in modport: "if_wire"`. Legal code rejected -- HLC bug, not a missing diagnostic |
| Modport.sv | 27, 29 | modport naming a declared `wire` and an implicit net | 25.5, 6.10 | - | as above | - | FALSE-POSITIVE | COMP_MODPORT_UNDEFINED_PORT | ERROR | COMP | - | Two CP0311 errors: `mp_net` (declared line 22) and `mp_implicit` (6.10 implicit net). Variable member `mp_var` resolves fine -- the bug is specific to **net** members |
| NetKeywords.sv | - | all 12 net_type keywords declared | 6.7 | - | -- | - | LEGAL | - | - | - | - | 0 diagnostics |
| Package.sv | - | `logic`/`reg`/`wire` declarations at package scope | 26.2 | `package_or_generate_item_declaration` (A.1.11) (satisfied) | the production lists `net_declaration` and `data_declaration` | - | LEGAL | - | - | - | - | `net_declaration` is a permitted package item, so a `wire` in a package is legal |
| Ports.sv | - | ANSI ports with and without an explicit type | 23.2.2.3 | - | as dut.sv | - | LEGAL | - | - | - | - | The file's "implicit net port" comments match 23.2.2.3 |
| Program.sv | 25, 28 | `wire` declaration and `assign` inside a program | 24.3 | `non_port_program_item` (A.1.7) (satisfied) | `non_port_program_item ::= { attribute_instance } continuous_assign \| ... module_or_generate_item_declaration \| ...` | - | LEGAL | - | - | - | - | Checked because 3.4's prose omits continuous assigns; the production admits them explicitly, so both lines are legal in a program |
| SecondModule.sv | - | interface + class + module instantiating both | - | - | -- | - | LEGAL | - | - | - | - | Re-declares `nets_and_variables_if`/`_class` from Interface.sv/Class.sv; see Compilation-model caveat |
| Var.sv | 42, 45 | `var logic var_initialized = 1'b0;` **and** `assign var_initialized = a & b;` | 6.5, 10.5 | - (semantic, not grammatical) | "it shall be an error to have multiple continuous assignments or a mixture of procedural and continuous assignments writing to any term in the expansion of the longest static prefix of a variable" (6.5); "The variable declaration assignment is a special case of procedural assignment" (10.5) | 48 | ADDED | HLDB_MIXED_PROCEDURAL_AND_CONT_ASSIGN | ERROR | DB | variable "var_initialized" in module "var_keyword_test"; extra location = the continuous assignment | UNCLAIMED. The fixture calls line 45 "legal: SystemVerilog allows assign to target a variable" -- true alone, but 10.5 makes the line-42 initializer a *procedural* assignment, so together they are the 6.5 mixture. HLC reports 0 diagnostics. Boundary: a variable with only one continuous assignment (`assign y = var_logic;` line 71) stays legal, as does one with only procedural writes |
| Variables.sv | - | every variable data type | 6.8 | - | -- | - | LEGAL | - | - | - | - | 0 diagnostics |

## NonAnsi

| File | Line | Construct / trigger | Clause | Grammar production | Normative wording | Catalog row | Verdict | Error code | Severity | Pass | Payload | Notes |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Assertion.sv | - | `int` property-local variable | 16.10 | `assertion_variable_declaration` (A.2.10) (satisfied) | as Ansi/Assertion.sv | - | LEGAL | - | - | - | - | |
| Behavior.sv | 37 | `var_reg = {var_reg[6:0], a}` where `var_reg` is 1 bit | 11.5.1 | - | an out-of-bounds part-select yields x; not an error | - | LEGAL | - | - | - | - | Checked deliberately: the part-select is out of range for a scalar `reg`, but the standard makes that an x-value, not a diagnostic |
| Checker.sv | 28 | `rand` checker variable | 17.2 | `checker_or_generate_item_declaration` (A.1.8) (satisfied) | as Ansi/Checker.sv | - | LEGAL | - | - | - | - | |
| Checker.sv | 30 | `randc` checker variable | 17.2 | **`checker_or_generate_item_declaration` (A.1.8) -- admits `[ rand ]` only** | as Ansi/Checker.sv:22 | - | EXISTS | PA_SYNTAX_ERROR | SYNTAX | PARSE | - | UNCLAIMED; same rule and same fixture error as Ansi/Checker.sv:22 |
| Class.sv | - | `rand`/`randc` class properties | 18.4 | `class_property` (A.1.9) (satisfied) | as Ansi/Class.sv | 600 | LEGAL | - | - | - | - | |
| Implicit.sv | - | implicit net by continuous assignment | 6.10 | - | as Ansi/Implicit.sv | - | LEGAL | - | - | - | - | |
| Instantiation.sv | 64 | a program instantiated inside a module | 24.3, 3.4 | `module_common_item` (A.1.4) (satisfied) | the production lists `program_instantiation`; 3.4 forbids instances *inside a program*, not programs inside modules | - | LEGAL | - | - | - | - | Checked because it looks like the 3.4 restriction; the direction is the opposite one |
| Interface.sv | 24 | modport naming a declared `wire` | 25.5 | - | as Ansi/Interface.sv | - | FALSE-POSITIVE | COMP_MODPORT_UNDEFINED_PORT | ERROR | COMP | - | `[ERR:CP0311] ... "if_wire"` on legal code |
| Modport.sv | 27, 29 | modport naming a declared `wire` and an implicit net | 25.5, 6.10 | - | as Ansi/Modport.sv | - | FALSE-POSITIVE | COMP_MODPORT_UNDEFINED_PORT | ERROR | COMP | - | Two CP0311 errors on legal code |
| Nets.sv | - | explicit net declarations, non-ANSI ports | 6.7 | - | -- | - | LEGAL | - | - | - | - | Leaves `` `default_nettype none`` set at EOF; see Compilation-model caveat |
| Package.sv | - | `wire` at package scope | 26.2 | `package_or_generate_item_declaration` (A.1.11) (satisfied) | as Ansi/Package.sv | - | LEGAL | - | - | - | - | |
| Ports.sv | - | non-ANSI implicit net ports | 23.2.2.1 | - | -- | - | LEGAL | - | - | - | - | |
| Program.sv | 24, 27 | `wire` + `assign` in a program | 24.3 | `non_port_program_item` (A.1.7) (satisfied) | as Ansi/Program.sv | - | LEGAL | - | - | - | - | |
| Variables.sv | - | explicit variable declarations | 6.8 | - | -- | - | LEGAL | - | - | - | - | |

## DefaultNettype

| File | Line | Construct / trigger | Clause | Grammar production | Normative wording | Catalog row | Verdict | Error code | Severity | Pass | Payload | Notes |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| BadArgumentType.sv | 22 | `` `default_nettype logic`` | 22.8 | **`default_nettype_value` (Syntax 22-7)** | `default_nettype_value ::= wire \| tri \| tri0 \| tri1 \| wand \| triand \| wor \| trior \| trireg \| uwire \| none` | 812 | EXISTS | PA_SYNTAX_ERROR | SYNTAX | PARSE | - | Caught only because `logic` is a keyword token outside the grammar's value set: `[SNT:PA0207] mismatched input 'logic' expecting {'none','supply0','supply1','tri',...}`. Note the expecting-set wrongly includes supply0/supply1 |
| MissingArgument.sv | 20 | `` `default_nettype`` with no value | 22.8 | **`default_nettype_compiler_directive` (Syntax 22-7)** | `` `default_nettype default_nettype_value`` -- the value is not optional | 812 | EXISTS | PP_SYNTAX_ERROR / PA_SYNTAX_ERROR | SYNTAX | PP / PARSE | - | `[SNT:PP0106] mismatched input '\n' expecting SPACES` plus a parser cascade |
| Nettype.sv | - | `wire`, `tri`, `none` demonstrations; illegal cases only in comments | 22.8 | `default_nettype_value` (Syntax 22-7) (satisfied) | "When the `default_nettype is set to none, all nets shall be explicitly declared" | 813 | LEGAL | - | - | - | - | Live code is legal; the file keeps its illegal cases commented out |
| NoneGateTerminal.sv | 27 | undeclared identifier as a primitive gate terminal under `` `default_nettype none`` | 22.8, 6.10 | - (semantic, not grammatical) | "When the `default_nettype is set to none, all nets shall be explicitly declared. If a net is not explicitly declared, an error is generated" | 813 | EXISTS-UNWIRED | ELAB_ILLEGAL_IMPLICIT_NET | ERROR | COMP | net "undeclared_gate_net" in module "..._none_gate_terminal_test" | HLC reports 0 errors. `ELAB_ILLEGAL_IMPLICIT_NET` ("Illegal implicit net \"%s\"") is exactly this diagnostic and has **zero call sites** -- only its definition at ErrorDefinition.cpp:197. Do not mint a second code. Gate terminals are 6.10 bullet 2 |
| NoneInstancePort.sv | 30 | undeclared identifier in an instance port connection under `none` | 22.8, 6.10 | - (semantic, not grammatical) | as above | 813 | EXISTS-UNWIRED | ELAB_ILLEGAL_IMPLICIT_NET | ERROR | COMP | net "undeclared_net" in module "..._none_instance_port_test" | Same dead code; port connection lists are 6.10 bullet 2 |
| Supply0.sv | 26 | `` `default_nettype supply0`` | 22.8 | **`default_nettype_value` (Syntax 22-7) -- 11 alternatives; `supply0`/`supply1` are not among them** | as BadArgumentType.sv | 812 | ADDED | PA_DEFAULT_NETTYPE_INVALID_VALUE | ERROR | PARSE | "supply0" | UNCLAIMED. The fixture asserts "the net type itself is still a legal `default_nettype value" -- it is not. HLC accepts it silently (0 diagnostics) because **its own grammar admits supply0/supply1**; that value set must shrink as part of wiring this code |
| Tri0.sv | - | `` `default_nettype tri0`` | 22.8 | `default_nettype_value` (Syntax 22-7) (satisfied) | `tri0` is in the value list | 812 | LEGAL | - | - | - | - | |
| Tri1.sv | - | `` `default_nettype tri1`` | 22.8 | `default_nettype_value` (Syntax 22-7) (satisfied) | in the list | 812 | LEGAL | - | - | - | - | |
| Triand.sv | - | `` `default_nettype triand`` | 22.8 | `default_nettype_value` (Syntax 22-7) (satisfied) | in the list | 812 | LEGAL | - | - | - | - | |
| Trior.sv | - | `` `default_nettype trior`` | 22.8 | `default_nettype_value` (Syntax 22-7) (satisfied) | in the list | 812 | LEGAL | - | - | - | - | |
| Trireg.sv | - | `` `default_nettype trireg`` | 22.8 | `default_nettype_value` (Syntax 22-7) (satisfied) | in the list | 812 | LEGAL | - | - | - | - | |
| UnrecognizedArgument.sv | 21 | `` `default_nettype mytype`` | 22.8 | **`default_nettype_value` (Syntax 22-7)** | as Supply0.sv | 812 | ADDED | PA_DEFAULT_NETTYPE_INVALID_VALUE | ERROR | PARSE | "mytype" | HLC accepts it silently (0 diagnostics) -- an arbitrary `SIMPLE_IDENTIFIER` slips through where a keyword like `logic` is caught |
| Uwire.sv | - | `` `default_nettype uwire`` | 22.8 | `default_nettype_value` (Syntax 22-7) (satisfied) | in the list | 812 | LEGAL | - | - | - | - | |
| Wand.sv | - | `` `default_nettype wand`` | 22.8 | `default_nettype_value` (Syntax 22-7) (satisfied) | in the list | 812 | LEGAL | - | - | - | - | |
| Wor.sv | - | `` `default_nettype wor`` | 22.8 | `default_nettype_value` (Syntax 22-7) (satisfied) | in the list | 812 | LEGAL | - | - | - | - | |
| WrongCase.sv | 21 | `` `default_nettype Wire`` | 22.8 | **`default_nettype_value` (Syntax 22-7)** | as Supply0.sv; keywords are case-sensitive (5.6.1) | 812 | ADDED | PA_DEFAULT_NETTYPE_INVALID_VALUE | ERROR | PARSE | "Wire" | HLC accepts it silently (0 diagnostics) |

## illegal_construct

| File | Line | Construct / trigger | Clause | Grammar production | Normative wording | Catalog row | Verdict | Error code | Severity | Pass | Payload | Notes |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| AssertionNet.sv | 24 | `wire local_val;` as a property-local variable | 16.10 | **`assertion_variable_declaration` (A.2.10) via `var_data_type` (A.2.2.1)** | `assertion_variable_declaration ::= var_data_type list_of_variable_decl_assignments ;` and `var_data_type ::= data_type \| var data_type_or_implicit` -- a net_type is neither | - | EXISTS | PA_SYNTAX_ERROR | SYNTAX | PARSE | - | `[SNT:PA0207] extraneous input 'wire'`. Not in catalog. Also emits `LN0813`/`LN0814` orphan/weak-node errors as post-syntax-error noise |
| ModuleRand.sv | 24 | `rand logic [3:0] mod_rand;` at module scope | 18.4 | **`data_declaration` (A.2.1.3); `random_qualifier` exists only in `class_property` (A.1.9)** | `data_declaration ::= [ const ] [ var ] [ lifetime ] data_type_or_implicit list_of_variable_decl_assignments ;` -- no qualifier slot | - | EXISTS | PA_SYNTAX_ERROR | SYNTAX | PARSE | - | `[SNT:PA0207] ... 'rand'`. Module items reach `data_declaration` with no random_qualifier available, so this is grammar-level. Not in catalog (rows 600-602 are class-scoped) |
| ModuleRandc.sv | 24 | `randc logic [1:0] mod_randc;` at module scope | 18.4 | **`data_declaration` (A.2.1.3); `random_qualifier` only in `class_property` (A.1.9)** | as above | - | EXISTS | PA_SYNTAX_ERROR | SYNTAX | PARSE | - | Same rule as ModuleRand.sv |
| Package.sv | 24 | `assign pkg_wire = pkg_logic;` at package scope | 26.2 | **`package_item` / `package_or_generate_item_declaration` (A.1.11)** | the 15 alternatives include `net_declaration` and `data_declaration` but **not** `continuous_assign` | - | EXISTS | PA_SYNTAX_ERROR | SYNTAX | PARSE | - | `[SNT:PA0207] extraneous input 'assign'`. The fixture's stated reason (packages have no process context) is loose, but the conclusion is right on grammar grounds. Not in catalog |
| ProceduralAssignmentUndeclared.sv | 26 | `undeclared_var = a;` in an `always` block, never declared | 6.10 | - (semantic, not grammatical -- the statement parses fine) | implicit nets are assumed in exactly three positions: port expression declaration, instance/primitive terminal list, LHS of a *continuous* assignment; a procedural assignment is none of them | - | EXISTS-UNWIRED | COMP_UNDEFINED_VARIABLE | ERROR | COMP | variable "undeclared_var" in module "..._procedural_assignment_undeclared_test" | HLC reports 0 errors -- silently accepts it. `COMP_UNDEFINED_VARIABLE` ("Undefined variable \"%s\"") is the right code and *is* live (Phase2ModelBuilder.cpp:20969) but does not fire for this shape. Do not mint |
| ProgramAlways.sv | 23 | `always` inside a program | 3.4 | **`non_port_program_item` (A.1.7) -- no `always_construct` alternative** | "A program block can contain data declarations, class definitions, subroutine definitions, object instances, and one or more initial or final procedures. It cannot contain always procedures, primitive instances, module instances, interface instances, or other program instances" | 2, 895 | EXISTS | PA_SYNTAX_ERROR | SYNTAX | PARSE | - | `[SNT:PA0207] extraneous input 'always'`. The omission is deliberate: `module_common_item` (A.1.4) lists `always_construct`, `non_port_program_item` keeps `initial_construct`/`final_construct` and drops it. Catalog row 2 already marked PASS |
| ProgramAlwaysComb.sv | 23 | `always_comb` inside a program | 3.4 | **`non_port_program_item` (A.1.7)** | as above | 2, 895 | EXISTS | PA_SYNTAX_ERROR | SYNTAX | PARSE | - | Same rule -- one rule, four files |
| ProgramAlwaysFf.sv | 24 | `always_ff` inside a program | 3.4 | **`non_port_program_item` (A.1.7)** | as above | 2, 895 | EXISTS | PA_SYNTAX_ERROR | SYNTAX | PARSE | - | Same rule |
| ProgramAlwaysLatch.sv | 24 | `always_latch` inside a program | 3.4 | **`non_port_program_item` (A.1.7)** | as above | 2, 895 | EXISTS | PA_SYNTAX_ERROR | SYNTAX | PARSE | - | Same rule; also emits `LN0813`/`LN0814` noise |

---

## Handoff -- input to sv-error-fix, one bullet per run

Both codes below are registered in `ErrorDefinition.h`/`.cpp` and compile, but **no code
raises either of them**. Neither changes HLC behavior until wired.

- **`PA_DEFAULT_NETTYPE_INVALID_VALUE = 216`** (ERROR, PARSE)
  `"Invalid default_nettype value: %s, legal values: wire, tri, tri0, tri1, wand, triand, wor, trior, trireg, uwire, none"`
  - Rule: 22.8 -- the argument must be one of the 11 `default_nettype_value` alternatives
    (Syntax 22-7; the standard notes this production is not in Annex A).
  - Payload: the offending value text, e.g. `"supply0"`. Location comes free from the
    directive's own file/line/column; there is no enclosing design element to name, since
    22.8 requires the directive to sit outside design elements.
  - Precedent to copy: `PA_UNCONNECTED_DRIVE_VALUE` (210) and `PA_TIMESCALE_INVALID_VALUE`
    (211) are the same shape -- validating a directive's argument, message carrying the
    legal set.
  - **Also required:** HLC's own grammar admits `supply0`/`supply1` in the value set
    (visible in the PA0207 expecting-set for `BadArgumentType.sv`) and admits any
    `SIMPLE_IDENTIFIER`. Wiring the check means tightening that set, not only adding the
    diagnostic.
  - Files that should start diagnosing: `DefaultNettype\Supply0.sv`,
    `DefaultNettype\UnrecognizedArgument.sv`, `DefaultNettype\WrongCase.sv`
    (and `BadArgumentType.sv` should move from a generic syntax error to this code).

- **`HLDB_MIXED_PROCEDURAL_AND_CONT_ASSIGN = 734`** (ERROR, DB)
  `"Mixed procedural and continuous assignments to %s"` + extra message `"%exloc other assignment"`
  - Rule: 6.5's second half -- a mixture of procedural and continuous assignments writing
    to any term in the longest static prefix of a variable. 10.5 supplies the missing
    premise: a variable declaration assignment *is* a procedural assignment.
  - Payload: `variable "<name>" in module "<enclosing>"`; pass the second assignment's
    site as an extra `Location` so `%exloc` renders it.
  - Sibling: `HLDB_MULTIPLE_CONT_ASSIGN` covers 6.5's *first* half (multiple continuous
    assignments) and is **also unwired** (zero call sites, ErrorDefinition.cpp:233). Wiring
    one is the natural moment to wire both -- they share a driver-accumulation walk.
  - Category note: catalog row 48 says `LINT`; the code went in the `HLDB_`/`DB` family
    because the other half of the same sentence already lives there with the same message
    shape. Re-argue this in sv-error-fix once the call site's real home is known.
  - Files that should start diagnosing: `Ansi\Var.sv`.

## Already-registered gaps found here (no new code -- call site only)

- **`ELAB_ILLEGAL_IMPLICIT_NET` (535)** -- defined, zero call sites. Would cover
  `DefaultNettype\NoneGateTerminal.sv` and `NoneInstancePort.sv` (22.8 `none` + an
  identifier in a 6.10 implicit-net position). Payload: `net "<name>" in module "<enclosing>"`.
- **`COMP_UNDEFINED_VARIABLE` (327)** -- live at `Phase2ModelBuilder.cpp:20969` but does
  not fire for a procedural assignment to an undeclared identifier. Would cover
  `illegal_construct\ProceduralAssignmentUndeclared.sv`.

## HLC bugs found -- legal code rejected (not diagnostic gaps)

`COMP_MODPORT_UNDEFINED_PORT` (CP0311) fires for every **net** named in a modport, while
variable members resolve correctly:

| File | Line | Member | Declared as |
|---|---|---|---|
| Ansi\Interface.sv | 22 | `if_wire` | `wire if_wire;` line 21 |
| NonAnsi\Interface.sv | 24 | `if_wire` | `wire if_wire;` line 21 |
| Ansi\Modport.sv | 27 | `mp_net` | `wire mp_net;` line 22 |
| Ansi\Modport.sv | 29 | `mp_implicit` | 6.10 implicit net, line 24 |
| NonAnsi\Modport.sv | 27 | `mp_net` | `wire mp_net;` line 22 |
| NonAnsi\Modport.sv | 29 | `mp_implicit` | 6.10 implicit net, line 24 |

IEEE 25.5 lets a modport expose nets as well as variables, so all six are legal. The
resolution path behind CP0311 appears to search only variables. This is a model/binder
defect, not a missing check -- out of scope for sv-error-fix.

A second, smaller defect: `AssertionNet.sv` and `ProgramAlwaysLatch.sv` also emit
`[ERR:LN0813] Found N orphan node(s)` / `[ERR:LN0814] Found N weak node(s)`, while
`ProgramAlways/Comb/Ff.sv` do not. Those are HLDB integrity-checker complaints caused by
the failed parse -- internal noise surfacing at ERROR severity from bad user input, and
inconsistently at that.

## Fixture corrections needed (davtests repo -- user's call)

Three fixtures assert that something illegal is legal. Nothing was edited here.

1. `Ansi\Checker.sv:22` and `NonAnsi\Checker.sv:30` -- `randc` on a checker variable is
   not legal (`checker_or_generate_item_declaration ::= [ rand ] data_declaration`, A.1.8);
   HLC already reports a syntax error, so both files currently fail to parse cleanly.
   Either drop the `randc` lines or move them to `illegal_construct\`.
2. `DefaultNettype\Supply0.sv` -- its header comment claims `supply0` "is still a legal
   `default_nettype value". It is not (22.8). The file belongs in `illegal_construct\`, or
   its comment must be inverted.
3. `Ansi\Var.sv:45` -- the comment "legal: SystemVerilog allows assign to target a
   variable" is true in isolation but wrong for this file, because line 42 already
   initializes the same variable (6.5 + 10.5).

## Compilation-model caveat

Design element names repeat across files: `nets_and_variables_test` in 5 Ansi files,
`nets_and_variables_nonansi` in 6 NonAnsi files, plus `nets_and_variables_program`,
`nets_and_variables_if`, `nets_and_variables_class` and their `_nonansi` variants in 2-3
files each. Under 3.13(a) a module/interface/program/primitive name "shall not be used
again in any compilation unit", so compiling these files together is itself an error
(catalog row 5, which HLC already diagnoses). Every measurement above therefore compiled
**one file per invocation**, which is what the fixtures' own comments intend
("so this file can be compiled and tested independently").

Related: `NonAnsi\*.sv` files end with `` `default_nettype none`` and the `Ansi\*.sv`
files carry no directive at all. In a single compilation unit the `none` would leak
forward and turn the legal implicit nets in `Ansi\Implicit.sv`, `Behavior.sv`,
`Interface.sv` and `Modport.sv` into 22.8 errors. Another reason the per-file model is the
only sound one for this tree.
