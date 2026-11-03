# davtests -- Skipped Test Audit

Inventory of every `GTEST_SKIP()` in the davtests C++ suite (`hlc/**/test_*.cpp`),
grouped by why the test is skipped. Use it to:

- find every test a given HLC fix should unblock, so the skips get removed
  in the same change;
- avoid adding a new skip for a gap that is already tracked here (reuse the
  wording and cross-reference instead);
- choose which HLC gaps to fix first by how many tests they unblock.

Audit date: 2026-10-16, davtests `master`. Categories were assigned from the
skip messages and surrounding comments; the suite was **not** built or run for
this audit, so treat counts as a snapshot and re-verify before acting.

---

## 1. How to regenerate

- Live skips: search `GTEST_SKIP()` in `hlc/**/*.cpp`, excluding lines whose
  first non-blank characters are `//`.
- On 2026-10-16 a raw search returned 419 hits; 81 of those were in comments,
  leaving **338 live skips in 327 tests across 245 files**.
- A skip inside an `if (...)` is *conditional*: it only fires while the gap
  exists. 42 skips are conditional; the rest are unconditional first
  statements of the test body.

---

## 2. Summary

| # | Category | Skips | Tests | Files | Cond. |
|---|---|---|---|---|---|
| 1 | Missing diagnostic (HLC accepts illegal code) | 113 | 113 | 48 | 3 |
| 2 | Simulation-only (asserts a runtime value) | 82 | 82 | 82 | 0 |
| 3 | Elaboration gap | 43 | 34 | 14 | 27 |
| 4 | Unverified expectation (test author unsure of correct shape) | 29 | 29 | 27 | 0 |
| 5 | Data model gap (not modeled / wrong shape) | 28 | 27 | 21 | 9 |
| 6 | Binding gap | 25 | 25 | 18 | 3 |
| 7 | Placeholder (skip with no assertions after it) | 7 | 7 | 6 | 0 |
| 8 | Parser / preprocessor gap | 5 | 5 | 5 | 0 |
| 9 | False-positive diagnostic | 4 | 4 | 3 | 0 |
| 10 | Unmodeled language feature | 2 | 2 | 2 | 0 |

Skips can exceed tests because a conditional test may contain several skips,
usually a primary one plus knock-on skips ("'pp2' itself was not elaborated").

---

## 3. Categories and root causes

### 3.1 Missing diagnostic (113)

HLC compiles code that IEEE 1800-2023 says is illegal, with no error. The test
asserts the error the standard requires.

- **ErrorCatalog `chapter-N/N--error_rules`** (about 85 skips). One test per
  catalog row, named `Row<N>_<Rule>`. Messages are either "not yet
  implemented in HLC's Linter (IEEE 1800-2023 X.Y)" or "no diagnostic
  implemented; ...". Heaviest: chapter 18 (constraints/randomize, 14),
  chapter 19 (covergroups, 14), chapter 16 (assertions, 8), chapter 10 (7),
  chapter 21 (5), chapter 11 (5).
- **Enum checks (6.19.x)**: sized literal width mismatch, x/z on a 2-state
  enum, unassigned name after an x/z name, assigning out-of-set values without
  a cast. Files: `Google/chapter-6/6.19*`, `EnumVarNoTypedef`.
- **Driver/declaration rules**: procedural assignment to a net (Table 10-1;
  `10.3--proc-assignment--bad`, `14.3--clocking-block-signals-error`),
  mixed or multiple continuous drivers on a variable (10.3.2), `reg`/`wire`
  redeclaration (6.5), duplicate declaration (`DefaultAssign`).
- **Undeclared identifiers not reported**: `default_nettype none` cases,
  procedural assignment to an undeclared name, chained selects on undeclared
  names (`HierBitSelect`), `PackageDpi`.
- **Other**: void function returning a value (13.4.1), bit-select of a
  `real` (6.12), zero-width indexed part-select (7.4.3), oversized stream
  unpack (11.4.14), undefined `extern constraint` prototypes (18.5.1),
  foreach loop variables exceeding the array rank, and sequence match items
  assigning non-local variables (16.10; `sequence5`, `sequence14`).
- `ErrorCatalog/chapter-15` Row460 is really a Linter wiring bug:
  `reportIllegalOutputArgumentLhs()` is only called from
  `checkFuncCall`/`checkTaskCall`, never from the method-call variants.

### 3.2 Simulation-only (82)

HLC is a static compiler/elaborator. These tests assert values that only exist
after simulation, e.g. a variable's value after an `initial` block runs, a
sort or reduction result, or `$write` output. **They will never pass in this
harness.** Located in `Google/chapter-7` (arrays/structs/unions runtime
tests), `chapter-9/9.4.2--*`, `chapter-10/10.4.1|10.6.*`, nearly all of
`chapter-11/*-sim` and their non-sim siblings, and `chapter-21/21.2--write`.

Many of these files put the real assertion after the skip and add a comment
"If the GTEST_SKIP() above is ever removed, this must still compile". Keep that
pattern. Do not count these toward "HLC gaps to fix".

### 3.3 Elaboration gap (43 skips, 34 tests)

- **Generate constructs not elaborated** (the largest cluster): the selected
  branch of a generate `if`/`case`, or the iterations of a generate `for`, are
  never instantiated. Files: `ForElab`, `GenerateUnnamed`, `GenerateAssigns`,
  `GenBlock`, `GenCase`, `BlackConst`. Most of these skips are conditional and
  chain off one another, so one fix unblocks the whole chain. Related:
  unlabeled generate blocks don't get the default `genblk<N>` name.
- **Gate arrays** not expanded into per-index instances (`Gates`).
- **Parameter overrides / defparam**: overridden ParamAssigns not kept
  (`ArianeElab`), defparam hierarchical indexed LHS fails to bind
  (`DefParamIndex`).
- **Constant-function folding**: ParamAssign RHS is neither a folded constant
  nor an unfolded FuncCall (`EvalFuncNamed`, `FuncCase`).
- "Only valid for elaborated model" (`Google/chapter-5/5.8-time-literals`, 7)
  and "Elaboration not yet implemented" (`Sequence/sequence15`, 6).
- Interconnect net typespec needs elaboration (`6.6.8--interconnect`).

### 3.4 Unverified expectation (29)

The test's expected value was inferred rather than confirmed, so it is unknown
whether the test or HLC is wrong. **Resolve these by checking the expected
value, not by changing HLC.**

- **Cycle-delay opcode (17)**: `vpiUnaryCycleDelayOp` vs `vpiCycleDelayOp` for
  binary `a ##N b`, plus the shape of `throughout`. Files:
  `Google/chapter-16/16.7--*`, `16.10--*`, `16.17--*`, `ExpectStmt`.
  Answering this one question settles all 17. See
  [sequence_expr_modeling.md](sequence_expr_modeling.md).
- **Intra-assignment timing controls (7)**: whether `#10`, `@(...)`,
  `repeat(n) @(...)` hang off `Assignment::getDelayControl()` /
  `getEventControl()` / `getRepeatControl()`. Files:
  `Google/chapter-9/9.4.5--*`; also the error type in `9.4.2.4--event_sequence`.
- **`any_cast` chains (3)**: `Google/chapter-9/9.7--process_cls_*`. It is not
  known which link of the chain breaks.

### 3.5 Data model gap (28)

- **Attributes** hoisted onto the Module instead of the declaration
  (`DataAttrib`; same bug documented in `5.12-attributes-variable`).
- **DPI C identifier** not defaulted to the SV name (`DpiChandle`, `DpiFunc`,
  `DpiTask`; 35.5.3).
- **VarSelect not modeled** (`FuncRetArray`, `FuncSideEffect`, `FuncStruct`).
- **Implicit nets never created** (5): `FilePackUnion`,
  `6.10--implicit_port_connection`, `PackageFuncCall` (3).
- **Lifetime defaults**: subroutine in `module automatic` defaults to static,
  and its locals don't default to automatic (`FuncStatic`).
- **Defaults not filled in**: implicit function return type (`FuncParam2`), enum
  base type default `int` (`8.16--cast_func`), nested-module time unit
  inheritance (`Timescale`).
- **Macro-expanded content** not represented (`BlackConst`: replication RHS,
  typedefs created by token pasting), escaped instance names (`Escape`),
  `$bits` part-select shape (`DollarBitsUnary`), `IODecl` for modules
  (`FuncBinding2`), `getFullName()` known broken (`GenScopeFullName`).

### 3.6 Binding gap (25)

- **Pattern identifiers (10)**: `v` bound by `tagged x '{.v, ...}` never
  resolves to its `AnyPattern`; the undeclared case expression is not
  reported either. Files: `Google/chapter-12/12.6.1--case*_pattern`,
  `12.6.2--if_pattern`.
- **Hierarchical references into named generate blocks**: `GenModHierPath`,
  `GenScopeFunc`, `HierPathInterfBlock`, `HierPathTfArg`.
- **Others**: `$root` paths (`DollarRoot`), forward-referenced package function
  (`EvalFuncPack`), out-of-block `new` without an extern prototype
  (`ExtendClassMember`), coverpoint receiver methods
  (`19--covergroup_methods`), string queue element receivers
  (`6.16--string_method_receivers`), `disable` of a sibling fork block
  (`9.6.2--disable_other`), UVM-side bindings (`18.13.3`, `18.13.4`),
  `ForeachClass`, ModPath vs RefObj ambiguity (`InterfaceIdentifiers`).

### 3.7 Placeholder (7)

These tests have a skip and nothing after it. The illegal construct is only
described in a comment in the `.sv` file. Files under `NetsAndVariables`
(`Ansi/Checker`, `NonAnsi/Checker`, `Ansi/Package`, `NonAnsi/Package`,
`Ansi/Var`, `DefaultNettype/Nettype`). To turn one into a real test, move the
construct into its own `.sv` file and assert the diagnostic, like the sibling
`DefaultNettype/None*` tests do.

### 3.8 Parser / preprocessor gap (5)

- `input` direction keyword in sequence formals rejected (PA0207;
  `sequence21`, `sequence22`).
- Escaped identifier as DPI linkage name not parsed (`35--error_rules` Row1108).
- Keyed assignment pattern on an LHS is a syntax error instead of the semantic
  error the standard requires (`10--error_rules` Row306).
- Directive generated from a macro argument is looked up as a macro name
  (`PreProcMacro`, PP0102).

### 3.9 False-positive diagnostic (4)

HLC reports errors on legal code: spurious null-actual errors
(`HierPathInterfBlock`, `HierPathTfArg`), spurious multiple-contassign and
"not yet implemented" errors (`PreprocUhdmCov`).

### 3.10 Unmodeled feature (2)

`randsequence` (`18--error_rules` Row663) and `with` on `reverse()`
(`7--error_rules` Row40).

---

## 4. Highest-leverage fixes

| Fix | Unblocks |
|---|---|
| Settle the cycle-delay opcode question (test-side) | 17 |
| Generate if/case/for elaboration | about 30 (cascading conditional skips) |
| Pattern-identifier binding (12.6.x) | 10 |
| Enum 6.19.x diagnostics | 6 |
| Implicit net creation | 5 (+ related `default_nettype none` diagnostics) |

---

## 5. Hygiene issues found

- **Stale "kept as GTEST_SKIP()" comments with no skip in the file.**
  Either the bug was fixed (update the comment) or the skip was dropped (the
  test now fails). Check each one:
  `Google/chapter-12/12.4.2--unique_if`, `12.4.2--unique0_if`,
  `12.4.2--priority_if`, `12.5.4--case_set`,
  `Google/chapter-6/6.10--implicit_continuous_assignment`,
  `Google/chapter-5/5.12-attributes-variable`.
- **Commented-out skips (16).** These tests now run and should pass; confirm.
  15 under `Google/chapter-7/arrays/packed/*` (querying functions, slice,
  equality, onebit, treat-as-integer, variable-slice*), plus
  `HierPathOverride`.
- **Likely typo**: `HierPathTfArg` `CompilerReportsZeroErrors` says "HLC does
  have a way to reliably check for null actuals" -- almost certainly "does
  not".
- **Messages too vague to act on.** These lack an IEEE section and the
  expected behavior required by `test_writing_guide.md`: `DefaultAssign`
  ("Linting errors aren't reported yet!"), `ForeachClass` ("Error reporting
  needs work..."), `5.8-time-literals` ("Only valid for elaborated model"),
  `PackageDpi`, `PackageFuncCall`, `PreprocUhdmCov`, `sequence15`,
  `InterfaceIdentifiers`.

---

## 6. When adding or removing a skip

- Follow davtests `.claude/test_writing_guide.md`: the skip is the first
  statement, assert the standard-correct behavior underneath it, and use the
  message form `"HLC does <wrong>; should <right> per IEEE 1800-2023 Sec <N>.
  Fix pending."`
- If the gap matches a cluster in Section 3, reuse that wording so a search
  finds every affected test.
- When an HLC fix lands, search for its cluster's wording and remove all the
  matching skips in the same change. Update the counts here.

---

## Appendix A -- Skipped tests by category

Paths are relative to the davtests repo root.

### A.1 Missing diagnostic

- `hlc/DefaultAssign/DefaultAssign/test_DefaultAssign.cpp`: `DuplicateDataDeclarationShouldBeRejected`
- `hlc/EnumVarNoTypedef/EnumVarNoTypedef/test_EnumVarNoTypedef.cpp`: `CompilerShouldRejectSizeMismatchButDoesNot`
- `hlc/ErrorCatalog/chapter-10/10--error_rules/test_10_error_rules.cpp`: `Row290_NonConstantSelectOnContinuousAssignLhsIsRejected`, `Row298_SelectIntoAtomicNetLhsIsRejected`, `Row309_AssignmentPatternInPortExpressionIsRejected`, `Row316_UnpackedArrayConcatOutsideAssignmentLikeContextIsRejected`, `Row322_ReplicationInsideUnpackedArrayConcatIsRejected`, `Row323_NestedUnpackedArrayConcatIsRejected`
- `hlc/ErrorCatalog/chapter-11/11--error_rules/test_11_error_rules.cpp`: `Row338_SelectOfConcatenationCannotBeAssignmentLhs`, `Row340_ReplicationCannotBeAssignmentLhs`, `Row341_ReplicationCannotConnectToOutputPort`, `Row346_StreamingConcatenationCannotBeBareOperand`, `Row369_EventTypedLetFormalReferencedOutsideEventExpressionContext`
- `hlc/ErrorCatalog/chapter-13/13--error_rules/test_13_error_rules.cpp`: `Row404_DiscardedNonvoidReturnValueIsWarned`, `Row420_PostReturnEventSchedulingOutsideInitialAlwaysForkThread`, `Row432_OmittedArgumentWithoutDefaultIsRejected`
- `hlc/ErrorCatalog/chapter-14/14--error_rules/test_14_error_rules.cpp`: `Row444_CycleDelayWithoutDefaultClockingIsRejected`, `Row450_GlobalClockWithNoEffectiveGlobalClockingIsRejected`, `Row453_ClockvarExpressionCannotBeAConcatenation`
- `hlc/ErrorCatalog/chapter-15/15--error_rules/test_15_error_rules.cpp`: `Row460_MailboxGetMessageArgumentMustBeAValidLhsExpression`
- `hlc/ErrorCatalog/chapter-16/16--error_rules/test_16_error_rules.cpp`: `Row474_NonStaticClassPropertyInAssertionIsRejected`, `Row476_SideEffectInAssertionExpressionIsRejected`, `Row478_FunctionWithStateOrSideEffectsInAssertionIsRejected`, `Row483_NamedSequenceInstanceMissingActualForFormalIsRejected`, `Row484_DollarActualBoundToFormalUsedAsOrdinaryOperandIsRejected`, `Row489_SequenceTypedFormalUsedAsGotoRepetitionOperandIsRejected`, `Row490_EventTypedFormalActualNotAnEventExpressionIsRejected`, `Row514_FutureSampledValueFunctionWithMatchItemsIsRejected`
- `hlc/ErrorCatalog/chapter-17/17--error_rules/test_17_error_rules.cpp`: `Row580_DollarActualRequiresUntypedFormalUsedAsDelayBound`, `Row582_CheckerOutputArgumentMustBeLvalue`, `Row597_FunctionInCheckerAssignmentMustBeSideEffectFree`
- `hlc/ErrorCatalog/chapter-18/18--error_rules/test_18_error_rules.cpp`: `Row608_ConstrainingAssociativeArraySizeIsRejected`, `Row613_ParenthesesOnConstraintPrimaryRequireArrayMethod`, `Row615_DistExpressionCannotNestInAnotherExpression`, `Row616_ExternalConstraintBlockMustFollowItsClass`, `Row629_RealDistRangeRequiresColonSlashWithWeight`, `Row630_DistDefaultSpecificationMustUseColonSlash`, `Row633_DistExpressionRequiresARandVariable`, `Row634_UniqueRangeListItemsMustBeSingularVariables`, `Row637_FunctionCallCannotBeForeachArrayIdentifier`, `Row646_ConstraintFunctionMustBeSideEffectFree`, `Row647_ConstraintFunctionCannotModifyConstraints`, `Row652_ScopeRandomizeCannotUseWithIdentifierListOrNull`, `Row665_RandsequenceRepeatCountMustBeNonNegative`, `Row666_RandJoinWeightMustBeInZeroToOneRange`
- `hlc/ErrorCatalog/chapter-19/19--error_rules/test_19_error_rules.cpp`: `Row672_CovergroupNewMustSupplyEveryNonDefaultedActual`, `Row680_CoverpointNameNotUsableForOptionAssignment`, `Row682_CovergroupExpressionConstantMustBeEnclosingClassMember`, `Row685_CovergroupExpressionFunctionMustHaveNoSideEffects`, `Row694_OnlyEnclosingCoverpointNameAllowedAsRangeList`, `Row696_CoverpointIdentifierNotVisibleInSetExpression`, `Row705_NegativeBinValueAgainstUnsignedCoverpointIsWarned`, `Row711_CrossNameNotUsableForOptionAssignment`, `Row713_OnlyEnclosingCrossIdentifierAllowedAsSelectExpression`, `Row719_OptionWeightMustBeNonNegative`, `Row720_ProceduralPerInstanceAssignmentIsRejected`, `Row721_AutoBinMaxOnlySettableInDefinition`, `Row725_StrobeOnlySettableInCovergroupDefinition`, `Row728_SampleFormalUsableOnlyInCoverpointOrGuard`
- `hlc/ErrorCatalog/chapter-20/20--error_rules/test_20_error_rules.cpp`: `Row731_FinishDiagnosticArgumentMustBe0Or1Or2`, `Row732_TimeformatUnitsNumberMustBeInRange`, `Row744_AssertcontrolControlTypeMustBeInTable20_5`
- `hlc/ErrorCatalog/chapter-21/21--error_rules/test_21_error_rules.cpp`: `Row754_FormatSpecifierWithoutAnArgumentIsRejected`, `Row762_FopenTypeMustBeATable21_6Form`, `Row765_SformatArgumentCountMustMatchFormatSpecifiers`, `Row782_OmittedDumpportsScopeListRequiresLeadingComma`, `Row786_DumpportslimitFilesizeArgumentIsRequired`
- `hlc/ErrorCatalog/chapter-23/23--error_rules/test_23_error_rules.cpp`: `Row862_RefPortCannotBeLeftUnconnected`, `Row864_InterfacePortCannotBeLeftUnconnected`
- `hlc/ErrorCatalog/chapter-25/25--error_rules/test_25_error_rules.cpp`: `Row912_RefDirectionModportSignalCannotBeSpecifyTerminal`
- `hlc/ErrorCatalog/chapter-26/26--error_rules/test_26_error_rules.cpp`: `Row942_AmbiguousWildcardImportedNameIsRejected`
- `hlc/ErrorCatalog/chapter-3/3--error_rules/test_3_error_rules.cpp`: `Row7_AttributeNameIsNotVisibleOutsideAttributeNameSpace`, `Row12_MixedTimescaleAndNoTimescaleElementsAreRejected`
- `hlc/ErrorCatalog/chapter-31/31--error_rules/test_31_error_rules.cpp`: `Row1034_SetupholdNegativeLimitPairMustExceedPrecision`, `Row1037_RecremNegativeLimitPairMustExceedPrecision`, `Row1052_AdjustedDelayedSignalLimitBelowZeroIsClampedWithWarning`
- `hlc/ErrorCatalog/chapter-35/35--error_rules/test_35_error_rules.cpp`: `Row1109_SameCIdentifierMustUseSameDpiVersionString`
- `hlc/ErrorCatalog/chapter-6/6--error_rules/test_6_error_rules.cpp`: `Row19_ForwardTypedefMismatchIsRejected`, `Row20_ScopeResolutionOnIncompleteForwardTypeIsRejected`, `Row27_MissingLifetimeKeywordIsRejected`, `Row25_StatementBeforeDeclarationIsRejected`
- `hlc/ErrorCatalog/chapter-8/8--error_rules/test_8_error_rules.cpp`: `Row201_NonStaticAccessFromStaticMethodIsRejected`, `Row236_NestedClassCannotImplicitlyAccessOuterNonStaticMember`, `Row239_OutOfBlockDeclarationMustFollowTheClass`
- `hlc/ForeachFunction/ForeachFunction/test_ForeachFunction.cpp`: `ForeachIndexCountExceedsArrayRank`
- `hlc/Google/chapter-10/10.3--proc-assignment--bad/test_10.3_proc-assignment--bad.cpp`: `CompilerShouldRejectProceduralAssignmentToWireButDoesNot`
- `hlc/Google/chapter-11/11.4.14.3--unpack_stream_inv/test_11.4.14.3_unpack_stream_inv.cpp`: `CompilerShouldRejectOversizedStreamUnpackButDoesNot`
- `hlc/Google/chapter-13/13.4.1--function-void-return/test_13.4.1_function-void-return.cpp`: `CompilerShouldRejectVoidFunctionReturningAValueButDoesNot`
- `hlc/Google/chapter-14/14.3--clocking-block-signals-error/test_14.3_clocking-block-signals-error.cpp`: `CompilerReportsAtLeastOneErrorForProceduralAssignmentToNet`
- `hlc/Google/chapter-18/18.5.1--explicit-external-constraint_1/test_18.5.1_explicit-external-constraint_1.cpp`: `UndefinedExternConstraintPrototypeShouldBeIllegalButIsAccepted`
- `hlc/Google/chapter-18/18.5.1--implicit-external-constraint_1/test_18.5.1_implicit-external-constraint_1.cpp`: `UndefinedConstraintPrototypeShouldBeIllegalButIsAccepted`
- `hlc/Google/chapter-5/5.7.1--integers-unsized-illegal/test_5.7.1--integers-unsized-illegal.cpp`: `Compiler_ShouldReportSyntaxErrorForIllegalToken`
- `hlc/Google/chapter-6/6.12--real_bit_select/test_6.12_real_bit_select.cpp`: `CompilerShouldRejectBitSelectOnRealVariableButDoesNot`
- `hlc/Google/chapter-6/6.19--enum_value_inv/test_6.19_enum_value_inv.cpp`: `CompilerShouldRejectSizeMismatchButDoesNot`
- `hlc/Google/chapter-6/6.19--enum_xx_inv/test_6.19_enum_xx_inv.cpp`: `CompilerShouldRejectXValueOn2StateEnumButDoesNot`
- `hlc/Google/chapter-6/6.19--enum_xx_inv_order/test_6.19_enum_xx_inv_order.cpp`: `CompilerShouldRejectUnassignedNameAfterXValueButDoesNot`
- `hlc/Google/chapter-6/6.19.3--enum_type_checking_inv/test_6.19.3_enum_type_checking_inv.cpp`: `CompilerShouldRejectUncastIntAssignmentButDoesNot`
- `hlc/Google/chapter-6/6.19.4--enum_numerical_expr_no_cast/test_6.19.4_enum_numerical_expr_no_cast.cpp`: `CompilerShouldRejectUncastCompoundAssignmentButDoesNot`
- `hlc/Google/chapter-6/6.5--variable_mixed_assignments/test_6.5_variable_mixed_assignments.cpp`: `CompilerShouldRejectMixedContinuousAndProceduralAssignmentButDoesNot`
- `hlc/Google/chapter-6/6.5--variable_multiple_assignments/test_6.5_variable_multiple_assignments.cpp`: `CompilerShouldRejectTwoContinuousAssignmentsToOneVariableButDoesNot`
- `hlc/Google/chapter-6/6.5--variable_redeclare/test_6.5_variable_redeclare.cpp`: `CompilerShouldRejectRedeclarationOfVButDoesNot`
- `hlc/Google/chapter-7/arrays/packed/variable-slice-zero/test_7.4.3_arrays_packed_variable-slice-zero.cpp`: `CompilerShouldRejectZeroWidthIndexedPartSelect`
- `hlc/HierBitSelect/HierBitSelect/test_HierBitSelect.cpp`: `MidoBytesInFailsToBind`, `StateDFailsToBind`, `StateD1FailsToBind`
- `hlc/NetsAndVariables/DefaultNettype/NoneGateTerminal/test_NetsAndVariables_DefaultNettype_NoneGateTerminal.cpp`: `UndeclaredGateTerminalNetIsRejected`
- `hlc/NetsAndVariables/DefaultNettype/NoneInstancePort/test_NetsAndVariables_DefaultNettype_NoneInstancePort.cpp`: `UndeclaredInstancePortNetIsRejected`
- `hlc/NetsAndVariables/DefaultNettype/UnrecognizedArgument/test_NetsAndVariables_DefaultNettype_UnrecognizedArgument.cpp`: `UnrecognizedArgumentIsRejected`
- `hlc/NetsAndVariables/DefaultNettype/WrongCase/test_NetsAndVariables_DefaultNettype_WrongCase.cpp`: `WrongCaseIsRejected`
- `hlc/NetsAndVariables/illegal_construct/ProceduralAssignmentUndeclared/test_NetsAndVariables_IllegalConstruct_ProceduralAssignmentUndeclared.cpp`: `ProceduralAssignmentToUndeclaredIsRejected`
- `hlc/PackageDpi/PackageDpi/test_PackageDpi.cpp`: `UndeclaredTypeUvmObjectWrapperIsReported`
- `hlc/Sequence/sequence14/test_sequence14.cpp`: `Seq14_MatchItems_OnlyModifyLocalVars`
- `hlc/Sequence/sequence5/test_sequence5.cpp`: `Seq5_MatchItems_OnlyModifyLocalVars`

### A.2 Simulation-only (runtime value)

- `hlc/Google/chapter-10/10.4.1--blocking-assignment/test_10.4.1_blocking-assignment.cpp`: `RuntimeBlockingAssignmentValuesRequireSimulation`
- `hlc/Google/chapter-10/10.6.1--assign-deassign/test_10.6.1_assign-deassign.cpp`: `RuntimeAssignDeassignOverrideRequiresSimulation`
- `hlc/Google/chapter-10/10.6.2--force-release/test_10.6.2_force-release.cpp`: `RuntimeForceReleaseOverrideRequiresSimulation`
- `hlc/Google/chapter-11/11.10--string_bit_array-sim/test_11.10_string_bit_array-sim.cpp`: `AEndsUpEqualToTest`
- `hlc/Google/chapter-11/11.10--string_bit_array/test_11.10_string_bit_array.cpp`: `AEndsUpEqualToTest`
- `hlc/Google/chapter-11/11.10.1--string_compare/test_11.10.1_string_compare.cpp`: `AAndBEndUpEqualToTest`
- `hlc/Google/chapter-11/11.10.1--string_concat/test_11.10.1_string_concat.cpp`: `ConcatenationContainsBothLiteralsAsSubstrings`
- `hlc/Google/chapter-11/11.10.1--string_copy/test_11.10.1_string_copy.cpp`: `AAndBEndUpEqualToTest`
- `hlc/Google/chapter-11/11.10.3--empty_string-sim/test_11.10.3_empty_string-sim.cpp`: `AEqualsZeroActuallyHoldsAtRuntime`
- `hlc/Google/chapter-11/11.10.3--empty_string/test_11.10.3_empty_string.cpp`: `AssertConditionActuallyHoldsAtRuntime`
- `hlc/Google/chapter-11/11.12--let_construct/test_11.12_let_construct.cpp`: `DEndsUpEqualToLetBodyResult`
- `hlc/Google/chapter-11/11.3.5--expr_short_circuit/test_11.3.5_expr_short_circuit.cpp`: `FunMustNeverExecuteBecauseAAndBAreBothTrue`
- `hlc/Google/chapter-11/11.3.6--assign_in_exp-sim/test_11.3.6_assign_in_exp-sim.cpp`: `BEndsUpEqualToCMinusOne`
- `hlc/Google/chapter-11/11.3.6--assign_in_expr-sim/test_11.3.6_assign_in_expr-sim.cpp`: `AllThreeVariablesEndUpEqualToFive`
- `hlc/Google/chapter-11/11.3.6--assign_in_expression-sim/test_11.3.6_assign_in_expression-sim.cpp`: `BEndsUpEqualToCPlusOne`
- `hlc/Google/chapter-11/11.3.6--assignment_in_expression-sim/test_11.3.6_assignment_in_expression-sim.cpp`: `BEndsUpEqualToCPlusOne`
- `hlc/Google/chapter-11/11.3.6--two_assign_in_expr-sim/test_11.3.6_two_assign_in_expr-sim.cpp`: `BAndDBothEndUpEqualToEPlusCPlusTwo`
- `hlc/Google/chapter-11/11.4.1--assignment-sim/test_11.4.1_assignment-sim.cpp`: `AEndsUpTwelveThenFive`
- `hlc/Google/chapter-11/11.4.10--arith-shift-assignment-signed/test_11.4.10_arith-shift-assignment-signed.cpp`: `BEqualsSixtyFourAndCEqualsNegativeFifteen`
- `hlc/Google/chapter-11/11.4.10--arith-shift-assignment-unsigned/test_11.4.10_arith-shift-assignment-unsigned.cpp`: `BEqualsSixtyFourAndCEqualsOne`
- `hlc/Google/chapter-11/11.4.10--arith-shift-signed/test_11.4.10_arith-shift-signed.cpp`: `BEqualsSixtyFourAndCEqualsNegativeFifteen`
- `hlc/Google/chapter-11/11.4.10--arith-shift-unsigned/test_11.4.10_arith-shift-unsigned.cpp`: `BEqualsSixtyFourAndCEqualsOne`
- `hlc/Google/chapter-11/11.4.11--cond_op-sim/test_11.4.11_cond_op-sim.cpp`: `CEndsUpEqualToEleven`
- `hlc/Google/chapter-11/11.4.12--concat_op-sim/test_11.4.12_concat_op-sim.cpp`: `AEndsUpEqualToHex8912`
- `hlc/Google/chapter-11/11.4.12.1--nested_repl_op-sim/test_11.4.12.1_nested_repl_op-sim.cpp`: `AEndsUpEqualToExpectedSixteenBitPattern`
- `hlc/Google/chapter-11/11.4.12.1--repl_op-sim/test_11.4.12.1_repl_op-sim.cpp`: `AEndsUpEqualToRepeatedPattern`
- `hlc/Google/chapter-11/11.4.12.2--string_concat_op/test_11.4.12.2_string_concat_op.cpp`: `StrEndsUpEqualToHelloWorldBang`
- `hlc/Google/chapter-11/11.4.12.2--string_repl_op/test_11.4.12.2_string_repl_op.cpp`: `StrEndsUpEqualToFourTimesTest`
- `hlc/Google/chapter-11/11.4.13--set_member-sim/test_11.4.13_set_member-sim.cpp`: `MembershipTestEvaluatesTrue`
- `hlc/Google/chapter-11/11.4.14.1--stream_concat-sim/test_11.4.14.1_stream_concat-sim.cpp`: `CEqualsAShiftedLeftThirtyTwoPlusB`
- `hlc/Google/chapter-11/11.4.14.1--stream_concat/test_11.4.14.1_stream_concat.cpp`: `CHoldsAInHighWordAndBInLowWord`
- `hlc/Google/chapter-11/11.4.14.2--reorder_stream-sim/test_11.4.14.2_reorder_stream-sim.cpp`: `BEqualsHexFourFourFourThreeFourTwoFourOne`
- `hlc/Google/chapter-11/11.4.14.2--reorder_stream/test_11.4.14.2_reorder_stream.cpp`: `BEndsUpByteReversedFromA`
- `hlc/Google/chapter-11/11.4.14.2--reorder_stream_byte-sim/test_11.4.14.2_reorder_stream_byte-sim.cpp`: `BEqualsHexFourFourFourThreeFourTwoFourOne`
- `hlc/Google/chapter-11/11.4.14.2--reorder_stream_byte/test_11.4.14.2_reorder_stream_byte.cpp`: `BEndsUpByteReversedFromA`
- `hlc/Google/chapter-11/11.4.14.4--dynamic_array_stream-sim/test_11.4.14.4_dynamic_array_stream-sim.cpp`: `HeaderLenAndCrcRoundTripAtRuntime`
- `hlc/Google/chapter-11/11.4.2--unary_op_dec-sim/test_11.4.2_unary_op_dec-sim.cpp`: `AEndsUpEqualToEleven`
- `hlc/Google/chapter-11/11.4.2--unary_op_inc-sim/test_11.4.2_unary_op_inc-sim.cpp`: `AEndsUpEqualToThirteen`
- `hlc/Google/chapter-11/11.4.5--equality-op/test_11.4.5_equality-op.cpp`: `AllSixComparisonsEvaluateToZero`
- `hlc/Google/chapter-11/11.5.1--idx_neg_part_select-sim/test_11.5.1_idx_neg_part_select-sim.cpp`: `BEqualsHex12AtRuntime`
- `hlc/Google/chapter-11/11.5.1--idx_pos_part_select-sim/test_11.5.1_idx_pos_part_select-sim.cpp`: `BEqualsHex34AtRuntime`
- `hlc/Google/chapter-11/11.5.1--idx_select-sim/test_11.5.1_idx_select-sim.cpp`: `BEqualsOneAndCEqualsZeroAtRuntime`
- `hlc/Google/chapter-11/11.5.1--non_idx_part_select-sim/test_11.5.1_non_idx_part_select-sim.cpp`: `BEqualsTwoAtRuntime`
- `hlc/Google/chapter-11/11.5.2--array_addressing-sim/test_11.5.2_array_addressing-sim.cpp`: `AEqualsOneTwentyFiveAtRuntime`
- `hlc/Google/chapter-11/11.5.2--multi_dim_array_addressing-sim/test_11.5.2_multi_dim_array_addressing-sim.cpp`: `AEqualsOneTwentyFiveAtRuntime`
- `hlc/Google/chapter-11/11.7--signed_func-sim/test_11.7_signed_func-sim.cpp`: `AEqualsNegativeEightAtRuntime`
- `hlc/Google/chapter-11/11.7--unsigned_func-sim/test_11.7_unsigned_func-sim.cpp`: `AEqualsTwoHundredFiftyTwoAtRuntime`
- `hlc/Google/chapter-11/11.9--tagged_union_member_access-sim/test_11.9_tagged_union_member_access-sim.cpp`: `BEqualsFortyTwoAtRuntime`
- `hlc/Google/chapter-11/11.9--tagged_union_member_access_inv/test_11.9_tagged_union_member_access_inv.cpp`: `ReadingInactiveTagShouldRaiseARuntimeError`
- `hlc/Google/chapter-21/21.2--write/test_21.2_write.cpp`: `WriteOmitsTheTrailingNewlineDisplayWouldAdd`
- `hlc/Google/chapter-7/arrays/associative/methods/delete/test_7.9.2_arrays_associative_methods_delete.cpp`: `DeleteRuntimeEffectOnMapSizeRequiresSimulation`
- `hlc/Google/chapter-7/arrays/associative/methods/first/test_7.9.4_arrays_associative_methods_first.cpp`: `RuntimeValuesOfRcAndSRequireSimulation`
- `hlc/Google/chapter-7/arrays/associative/methods/last/test_7.9.5_arrays_associative_methods_last.cpp`: `RuntimeValuesOfRcAndSRequireSimulation`
- `hlc/Google/chapter-7/arrays/associative/methods/next/test_7.9.6_arrays_associative_methods_next.cpp`: `RuntimeValuesOfRcAndSRequireSimulation`
- `hlc/Google/chapter-7/arrays/associative/methods/prev/test_7.9.7_arrays_associative_methods_prev.cpp`: `RuntimeValuesOfRcAndSRequireSimulation`
- `hlc/Google/chapter-7/arrays/associative/methods/traversal/test_7.9.8_arrays_associative_methods_traversal.cpp`: `RuntimeValuesOfRcAndIxRequireSimulation`
- `hlc/Google/chapter-7/arrays/multidimensional/copy/test_7.4.5_array_multidimensional_copy.cpp`: `RuntimeValuesRequireSimulation`
- `hlc/Google/chapter-7/arrays/multidimensional/subarrays/test_7.4.5_array_multidimensional_subarrays.cpp`: `RuntimeValuesRequireSimulation`
- `hlc/Google/chapter-7/arrays/unpacked/ordering-methods/reverse/test_7.12.2_arrays_unpacked_reverse.cpp`: `RuntimeReverseResultRequiresSimulation`
- `hlc/Google/chapter-7/arrays/unpacked/ordering-methods/rsort/test_7.12.2_arrays_unpacked_rsort.cpp`: `RuntimeRsortResultRequiresSimulation`
- `hlc/Google/chapter-7/arrays/unpacked/ordering-methods/shuffle/test_7.12.2_arrays_unpacked_shuffle.cpp`: `RuntimeShuffleResultIsNonDeterministicRequiresSimulation`
- `hlc/Google/chapter-7/arrays/unpacked/ordering-methods/sort/test_7.12.2_arrays_unpacked_sort.cpp`: `RuntimeSortResultRequiresSimulation`
- `hlc/Google/chapter-7/arrays/unpacked/reduction-methods/and/test_7.12.3_arrays_unpacked_and.cpp`: `RuntimeAndResultRequiresSimulation`
- `hlc/Google/chapter-7/arrays/unpacked/reduction-methods/or/test_7.12.3_arrays_unpacked_or.cpp`: `RuntimeOrResultRequiresSimulation`
- `hlc/Google/chapter-7/arrays/unpacked/reduction-methods/product/test_7.12.3_arrays_unpacked_product.cpp`: `RuntimeProductResultRequiresSimulation`
- `hlc/Google/chapter-7/arrays/unpacked/reduction-methods/sum/test_7.12.3_arrays_unpacked_sum.cpp`: `RuntimeSumResultRequiresSimulation`
- `hlc/Google/chapter-7/arrays/unpacked/reduction-methods/xor/test_7.12.3_arrays_unpacked_xor.cpp`: `RuntimeXorResultRequiresSimulation`
- `hlc/Google/chapter-7/structures/packed/basic/test_7.2.1_structures_packed_basic.cpp`: `RuntimePackedFieldValuesRequireSimulation`
- `hlc/Google/chapter-7/structures/packed/signed/test_7.2.1_structures_packed_signed.cpp`: `RuntimePackedSignedValueRequiresSimulation`
- `hlc/Google/chapter-7/structures/packed/unsigned/test_7.2.1_structures_packed_unsigned.cpp`: `RuntimePackedUnsignedValueRequiresSimulation`
- `hlc/Google/chapter-7/structures/unpacked/basic/test_7.2_structures_unpacked_basic.cpp`: `RuntimeUnpackedFieldValuesRequireSimulation`
- `hlc/Google/chapter-7/structures/unpacked/default-value/test_7.2.2_structures_unpacked_default-value.cpp`: `RuntimeDefaultValueRequiresSimulation`
- `hlc/Google/chapter-7/unions/basic/test_7.3_unions_basic.cpp`: `RuntimeUnionOverlapValuesRequireSimulation`
- `hlc/Google/chapter-7/unions/packed/basic/test_7.3.1_unions_packed_basic.cpp`: `RuntimeUnionOverlapValuesRequireSimulation`
- `hlc/Google/chapter-7/unions/tagged/basic/test_7.3.2_unions_tagged_basic.cpp`: `RuntimeTaggedUnionDisplayRequiresSimulation`
- `hlc/Google/chapter-7/unions/tagged/packed/test_7.3.2_unions_tagged_packed.cpp`: `RuntimeTaggedUnionDisplayRequiresSimulation`
- `hlc/Google/chapter-7/unions/unpacked/basic/test_7.3_unions_unpacked_basic.cpp`: `RuntimeUnionOverlapValuesRequireSimulation`
- `hlc/Google/chapter-9/9.4.2--event_control_edge/test_9.4.2--event_control_edge.cpp`: `RuntimeToggleShouldFlipAButHlcCannotSimulate`
- `hlc/Google/chapter-9/9.4.2--event_control_negedge/test_9.4.2--event_control_negedge.cpp`: `RuntimeToggleShouldFlipAButHlcCannotSimulate`
- `hlc/Google/chapter-9/9.4.2--event_control_posedge/test_9.4.2--event_control_posedge.cpp`: `RuntimeToggleShouldFlipAButHlcCannotSimulate`
- `hlc/Google/chapter-9/9.4.2--event_control_sim/test_9.4.2--event_control_sim.cpp`: `IFinalValueShouldBeThreeButHlcCannotSimulate`
- `hlc/Google/chapter-9/9.4.2--event_control_sim_minimal/test_9.4.2--event_control_sim_minimal.cpp`: `IFinalValueShouldBeOneButHlcCannotSimulate`

### A.3 Elaboration gap

- `hlc/ArianeElab/ArianeElab/test_ArianeElab.cpp`: `TopModuleArianeCfgOverrideParses`
- `hlc/BlackConst/BlackConst/test_BlackConst.cpp`: `GenerateIf_ElaboratesGoodInstance`
- `hlc/DefParamIndex/DefParamIndex/test_DefParamIndex.cpp`: `DefParam0_Lhs_ResolvesThroughInstanceHierarchy`
- `hlc/EvalFuncNamed/EvalFuncNamed/test_EvalFuncNamed.cpp`: `MyParam2ValueEvaluatesNamedCallOrRemainsUnfoldedFuncCall`
- `hlc/ForElab/ForElab/test_ForElab.cpp`: `UReqArbInstanceElaborated`, `GenNormalCaseElaborated`, `GenTreeHasThreeIterations`, `EachGenTreeIterationHasOrGateNamedGate`
- `hlc/FuncCase/FuncCase/test_FuncCase.cpp`: `TopParamPFoldsToTwoOrRemainsUnfoldedCall`
- `hlc/Gates/Gates/test_Gates.cpp`: `GateArrayElaboratesToEightGateInstances`
- `hlc/GenBlock/GenBlock/test_GenBlock.cpp`: `ElseBranchElaboratesGoodInstance`
- `hlc/GenCase/GenCase/test_GenCase.cpp`: `FirstItemBodyIsNamedScopeTag1`
- `hlc/GenerateAssigns/GenerateAssigns/test_GenerateAssigns.cpp`: `U2BranchElaborated`, `U1BranchElaboratedUnderU2`, `U1HasXnorContAssign`
- `hlc/GenerateUnnamed/GenerateUnnamed/test_GenerateUnnamed.cpp`: `Pp2BranchElaborated`, `CaseDefaultArmKeepsExplicitLabelU1`, `UnlabeledIfBlockGetsDefaultGenblkName`, `LabeledIfBlockKeepsExplicitLabelN5`, `PrimitiveG4IsXnorGate`
- `hlc/Google/chapter-5/5.8-time-literals/test_5.8-time-literals.cpp`: `Assignment0_1fs_ScaledPerSpec`, `Assignment1_1ps_ScaledPerSpec`, `Assignment2_1ns_ScaledPerSpec`, `Assignment3_1us_ScaledPerSpec`, `Assignment4_1ms_ScaledPerSpec`, `Assignment5_1s_ScaledPerSpec`, `Assignment6_2p1ms_ScaledPerSpec`
- `hlc/Google/chapter-6/6.6.8--interconnect/test_6.6.8_interconnect.cpp`: `TopNetHasLogicTypespec`
- `hlc/Sequence/sequence15/test_sequence15.cpp`: `Seq15_Expr_IsInsideOp`, `Seq15_Expr_IsNotDistribution`, `Seq15_InsideOp_HasOperands`, `Seq15_InsideOp_FirstOperandIsRefToA`, `Seq15_InsideOp_ValueSetContainsOne`, `Seq15_InsideOp_ValueSetContainsTwo`

### A.4 Unverified expectation

- `hlc/ExpectStmt/ExpectStmt/test_ExpectStmt.cpp`: `PropertyExprIsNonNullCycleDelayOfAAndB`
- `hlc/Google/chapter-16/16.10--sequence-local-var-fail/test_16.10--sequence-local-var-fail.cpp`: `SequenceExprIsCycleDelayedEqualityWithLocalVariable`
- `hlc/Google/chapter-16/16.10--sequence-local-var-uvm/test_16.10--sequence-local-var-uvm.cpp`: `SequenceExprIsCycleDelayedEqualityWithLocalVariable`
- `hlc/Google/chapter-16/16.10--sequence-local-var/test_16.10--sequence-local-var.cpp`: `SequenceExprIsCycleDelayedEqualityWithLocalVariable`
- `hlc/Google/chapter-16/16.17--expect-uvm-fail/test_16.17--expect-uvm-fail.cpp`: `PropertyExprIsCycleDelayOfTwoOfReadAndWrite`
- `hlc/Google/chapter-16/16.17--expect-uvm/test_16.17--expect-uvm.cpp`: `PropertyExprIsCycleDelayOfReadAndWrite`
- `hlc/Google/chapter-16/16.17--expect/test_16.17--expect.cpp`: `PropertyExprIsCycleDelayOfAAndB`
- `hlc/Google/chapter-16/16.7--sequence-and-range-uvm/test_16.7--sequence-and-range-uvm.cpp`: `SequenceExprContainsAndOfTwoRangedCycleDelaySubsequences`
- `hlc/Google/chapter-16/16.7--sequence-and-uvm/test_16.7--sequence-and-uvm.cpp`: `SequenceExprContainsAndOfTwoCycleDelaySubsequences`
- `hlc/Google/chapter-16/16.7--sequence-intersect-uvm-fail/test_16.7--sequence-intersect-uvm-fail.cpp`: `SequenceExprIsIntersectOfFixedAndRangedCycleDelaySubsequences`
- `hlc/Google/chapter-16/16.7--sequence-intersect-uvm/test_16.7--sequence-intersect-uvm.cpp`: `SequenceExprIsIntersectOfFixedAndRangedCycleDelaySubsequences`
- `hlc/Google/chapter-16/16.7--sequence-or-uvm/test_16.7--sequence-or-uvm.cpp`: `SequenceExprIsOrOfTwoCycleDelaySubsequences`
- `hlc/Google/chapter-16/16.7--sequence-throughout-uvm-fail/test_16.7--sequence-throughout-uvm-fail.cpp`: `SequenceSeqIsSingleCycleDelay`, `AssertPropertyIsThroughoutOfGnt2AndSeq`
- `hlc/Google/chapter-16/16.7--sequence-throughout-uvm/test_16.7--sequence-throughout-uvm.cpp`: `SequenceSeqIsIntersectOfFixedAndRangedCycleDelaySubsequences`, `AssertPropertyIsThroughoutOfGnt2AndSeq`
- `hlc/Google/chapter-16/16.7--sequence-uvm-fail/test_16.7--sequence-uvm-fail.cpp`: `SequenceExprIsSingleCycleDelayOnReadSignal`
- `hlc/Google/chapter-16/16.7--sequence-uvm/test_16.7--sequence-uvm.cpp`: `SequenceExprIsSingleCycleDelay`
- `hlc/Google/chapter-9/9.4.2.4--event_sequence/test_9.4.2.4--event_sequence.cpp`: `ProceduralAssignmentToWiresAYAndClkIsIllegal`
- `hlc/Google/chapter-9/9.4.5--event_blocking_assignment_delay/test_9.4.5--event_blocking_assignment_delay.cpp`: `AssignmentCarriesInlineDelayControl`
- `hlc/Google/chapter-9/9.4.5--event_nonblocking_assignment_delay/test_9.4.5--event_nonblocking_assignment_delay.cpp`: `AssignmentIsNonblockingWithInlineDelayControl`
- `hlc/Google/chapter-9/9.4.5--event_nonblocking_assignment_event/test_9.4.5--event_nonblocking_assignment_event.cpp`: `AssignmentIsActuallyBlockingWithInlineEventControl`
- `hlc/Google/chapter-9/9.4.5--event_nonblocking_assignment_repeat/test_9.4.5--event_nonblocking_assignment_repeat.cpp`: `AssignmentCarriesRepeatControlWithLiteralCountThree`
- `hlc/Google/chapter-9/9.4.5--event_nonblocking_assignment_repeat_int/test_9.4.5--event_nonblocking_assignment_repeat_int.cpp`: `AssignmentCarriesRepeatControlWithVariableCountI`
- `hlc/Google/chapter-9/9.4.5--event_nonblocking_assignment_repeat_int_neg/test_9.4.5--event_nonblocking_assignment_repeat_int_neg.cpp`: `AssignmentCarriesRepeatControlWithVariableCountI`
- `hlc/Google/chapter-9/9.4.5--event_nonblocking_assignment_repeat_neg/test_9.4.5--event_nonblocking_assignment_repeat_neg.cpp`: `AssignmentCarriesRepeatControlWithLiteralNegativeThree`
- `hlc/Google/chapter-9/9.7--process_cls_kill/test_9.7--process_cls_kill.cpp`: `FourTopLevelStatementsForkThenWaitThenAwaitThenConditionalKill`
- `hlc/Google/chapter-9/9.7--process_cls_self/test_9.7--process_cls_self.cpp`: `FirstStatementIsForeachOverJobWithForkJoinNone`
- `hlc/Google/chapter-9/9.7--process_cls_suspend_resume/test_9.7--process_cls_suspend_resume.cpp`: `FiveTopLevelStatementsForkWaitResumeAwaitConditionalKill`

### A.5 Data model gap

- `hlc/BlackConst/BlackConst/test_BlackConst.cpp`: `Easy_RhsIsMultiConcatOperation`, `IcacheStructTypedefs_ExistAndArePackedStructs`
- `hlc/DataAttrib/DataAttrib/test_DataAttrib.cpp`: `ModuleHasNoAttributes`, `MyReg1HasPreserveFlagAttribute`, `MyReg2HasPreserveFlagAttribute`
- `hlc/DollarBitsUnary/DollarBitsUnary/test_DollarBitsUnary.cpp`: `BitsArgumentOfSelectRangeIsBitNegOfDrQ`
- `hlc/DpiChandle/DpiChandle/test_DpiChandle.cpp`: `TestOutputDpiCIdentifierDefaultsToItsSvName`
- `hlc/DpiFunc/DpiFunc/test_DpiFunc.cpp`: `ExportDpiCIdentifierDefaultsToItsSvName`
- `hlc/DpiTask/DpiTask/test_DpiTask.cpp`: `ExportDpiCIdentifierDefaultsToItsSvName`
- `hlc/Escape/Escape/test_Escape.cpp`: `EscapedInstanceNameHasBackslashStripped`, `EscapedInstanceNameDoesNotContainBackslash`
- `hlc/EvalFuncPack/EvalFuncPack/test_EvalFuncPack.cpp`: `InfosPerBankForwardReferencesMaxInfoPages`
- `hlc/FilePackUnion/FilePackUnion/test_FilePackUnion.cpp`: `UndeclaredCBecomesImplicitNet`
- `hlc/FuncBinding2/FuncBinding2/test_FuncBinding2.cpp`: `NewspaperIsAWireAndClockIsAnInputPort`
- `hlc/FuncParam2/FuncParam2/test_FuncParam2.cpp`: `FuncDefaultReturnTypeIsLogicIfModeled`
- `hlc/FuncRetArray/FuncRetArray/test_FuncRetArray.cpp`: `FuncBodyIsForLoopAssigningEachElementOfTheReturnArray`
- `hlc/FuncSideEffect/FuncSideEffect/test_FuncSideEffect.cpp`: `PowFlipBBodyAssignsToModuleScopeOut2AsASideEffect`
- `hlc/FuncStatic/FuncStatic/test_FuncStatic.cpp`: `Accumulate2InheritsAutomaticFromModuleDefault`
- `hlc/FuncStruct/FuncStruct/test_FuncStruct.cpp`: `CompressBodyAssignsSigmaZeroThenElementZeroOfItsOwnReturnName`
- `hlc/GenScopeFullName/GenScopeFullName/test_GenScopeFullName.cpp`: `GenForFullNameIsHierarchicalPath`
- `hlc/Google/chapter-6/6.10--implicit_port_connection/test_6.10_implicit_port_connection.cpp`: `NoFailedToBindErrorReportedForImplicitNetC`
- `hlc/Google/chapter-8/8.16--cast_func/test_8.16--cast_func.cpp`: `EnumTypespecBaseTypeDefaultsToInt`
- `hlc/NetsAndVariables/Ansi/Checker/test_NetsAndVariables_Ansi_Checker.cpp`: `CheckerHasRandVariables`
- `hlc/PackageFuncCall/PackageFuncCall/test_PackageFuncCall.cpp`: `DataStateSboxIsTheOnlyImplicitScalarWire`, `TopHasOneContAssignDrivingImplicitNet`, `EveryUndeclaredIdentifierIsReported`
- `hlc/Timescale/Timescale/test_Timescale.cpp`: `M12InheritsTimeUnitFromEnclosingModule`

### A.6 Binding gap

- `hlc/DollarRoot/DollarRoot/test_DollarRoot.cpp`: `TopContAssignLhsResolvesToPortA`, `UnresolvableRootTbReferenceFailsToBindInTestProgram1`
- `hlc/ErrorCatalog/chapter-19/19--covergroup_methods/test_19_covergroup_methods.cpp`: `CoverageQueriesBindOnACoverpointReceiver`
- `hlc/ErrorCatalog/chapter-6/6.16--string_method_receivers/test_6.16_string_method_receivers.cpp`: `QueueElementReceiverBindsToStringTypespec`
- `hlc/EvalFuncPack/EvalFuncPack/test_EvalFuncPack.cpp`: `InfosPerBankForwardReferencesMaxInfoPages`
- `hlc/ExtendClassMember/ExtendClassMember/test_ExtendClassMember.cpp`: `OutOfBlockNewMemberAccessResolution`
- `hlc/ForeachClass/ForeachClass/test_ForeachClass.cpp`: `RangeMinMaxFailToResolve`
- `hlc/GenModHierPath/GenModHierPath/test_GenModHierPath.cpp`: `ReadmemhArgumentIsHierPathToArray`
- `hlc/GenScopeFunc/GenScopeFunc/test_GenScopeFunc.cpp`: `ContAssignCallsLocalFunctionBoundToCscope`
- `hlc/Google/chapter-12/12.6.1--case_pattern/test_12.6.1_case_pattern.cpp`: `CaseExpressionVIsUndeclaredAndFailsToBind`, `FirstItemDisplayReferencesPatternBoundV`, `FourthItemDisplayReferencesPatternBoundV`
- `hlc/Google/chapter-12/12.6.1--casex_pattern/test_12.6.1_casex_pattern.cpp`: `CaseExpressionVIsUndeclaredAndFailsToBind`, `FirstItemDisplayReferencesPatternBoundV`, `FourthItemDisplayReferencesPatternBoundV`
- `hlc/Google/chapter-12/12.6.1--casez_pattern/test_12.6.1_casez_pattern.cpp`: `CaseExpressionVIsUndeclaredAndFailsToBind`, `FirstItemDisplayReferencesPatternBoundV`, `FourthItemDisplayReferencesPatternBoundV`
- `hlc/Google/chapter-12/12.6.2--if_pattern/test_12.6.2_if_pattern.cpp`: `TrueArmDisplayReferencesPatternBoundV`
- `hlc/Google/chapter-18/18.13.3--srandom_0/test_18.13.3--srandom_0.cpp`: `EveryCalledOrExtendedNameBinds`
- `hlc/Google/chapter-18/18.13.4--get_randstate_0/test_18.13.4--get_randstate_0.cpp`: `EveryCalledOrExtendedNameBinds`
- `hlc/Google/chapter-9/9.6.2--disable_other/test_9.6.2--disable_other.cpp`: `DisableTargetsTheSiblingNamedBlock`
- `hlc/HierPathInterfBlock/HierPathInterfBlock/test_HierPathInterfBlock.cpp`: `NoneOfTheEightHierPathCallsFailToBind`
- `hlc/HierPathTfArg/HierPathTfArg/test_HierPathTfArg.cpp`: `HierarchicalCallsShouldResolveToTheirDeclaredFunction`
- `hlc/InterfaceIdentifiers/InterfaceIdentifiers/test_InterfaceIdentifiers.cpp`: `SpecifyPathsModuleHasModPaths`

### A.7 Placeholder (no assertions)

- `hlc/NetsAndVariables/Ansi/Checker/test_NetsAndVariables_Ansi_Checker.cpp`: `CkWireIsIllegalInsideChecker`
- `hlc/NetsAndVariables/Ansi/Package/test_NetsAndVariables_Ansi_Package.cpp`: `ContAssignIsIllegalInsidePackage`
- `hlc/NetsAndVariables/Ansi/Var/test_NetsAndVariables_Ansi_Var.cpp`: `VarNetTypeCombinationsAreIllegalNotCompiled`
- `hlc/NetsAndVariables/DefaultNettype/Nettype/test_NetsAndVariables_DefaultNettype_Nettype.cpp`: `ImplicitNetUnderNoneIsIllegalNotCompiled`, `NonAnsiUntypedPortsUnderNoneIsIllegalNotCompiled`
- `hlc/NetsAndVariables/NonAnsi/Checker/test_NetsAndVariables_NonAnsi_Checker.cpp`: `CkWireIsIllegalInsideChecker`
- `hlc/NetsAndVariables/NonAnsi/Package/test_NetsAndVariables_NonAnsi_Package.cpp`: `ContAssignIsIllegalInsidePackage`

### A.8 Parser/preprocessor

- `hlc/ErrorCatalog/chapter-10/10--error_rules/test_10_error_rules.cpp`: `Row306_KeyedNotationOnAssignmentPatternLhsIsRejected`
- `hlc/ErrorCatalog/chapter-35/35--error_rules/test_35_error_rules.cpp`: `Row1108_LinkageNameThatIsNotACIdentifierIsRejected`
- `hlc/PreProcMacro/PreProcMacro/test_PreProcMacro.cpp`: `DirectiveGeneratedFromMacroArgumentCompilesCleanly`
- `hlc/Sequence/sequence21/test_sequence21.cpp`: `Compiler_NoSyntaxErrors`
- `hlc/Sequence/sequence22/test_sequence22.cpp`: `Compiler_NoSyntaxErrors`

### A.9 False-positive diagnostic

- `hlc/HierPathInterfBlock/HierPathInterfBlock/test_HierPathInterfBlock.cpp`: `CompilerReportsZeroErrors`
- `hlc/HierPathTfArg/HierPathTfArg/test_HierPathTfArg.cpp`: `CompilerReportsZeroErrors`
- `hlc/PreprocUhdmCov/PreprocUhdmCov/test_PreprocUhdmCov.cpp`: `NoSpuriousMultipleContAssignOnA`, `PrimSubregInstantiationDoesNotReportUnsupportedTypespec`

### A.10 Unmodeled feature

- `hlc/ErrorCatalog/chapter-18/18--error_rules/test_18_error_rules.cpp`: `Row663_NegativeProductionWeightIsRejected`
- `hlc/ErrorCatalog/chapter-7/7--error_rules/test_7_error_rules.cpp`: `Row40_WithClauseOnReverseIsRejected`
