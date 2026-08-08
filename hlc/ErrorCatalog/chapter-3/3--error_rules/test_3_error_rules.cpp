/*
 Copyright 2020 Apotell

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
*/

// Tests for the IEEE 1800-2023 Clause 3 error scenarios catalogued in
// sv_error_catalog.md (rows 2-13).
//
// Scope: this file asserts ONLY that the diagnostic each catalog row requires
// is emitted. It deliberately makes no assertion about the shape of the
// compiled model -- no typespecs, no net-vs-variable, no recovery behaviour.
// Exactly one TEST_F per catalog row, named Row<N>_... after that row and
// carrying a "catalog row N | clause | category" comment; that is the link
// between the workbook and this file.
//
// Fixtures (all three compiled in one run by 3--error_rules.hlc):
//   3--error_rules.sv       rows 2, 4, 5, 6, 7, 8, 9, 10, 11, 13
//   3--error_rules_inv2.sv  row 3  -- a primitive instantiating a module
//   3--error_rules_inv3.sv  row 12 -- mixed timescale / no-timescale elements
//
// Only two rows need a fixture of their own, each for a demonstrated reason:
// row 3's illegal instantiation makes the parser report "mismatched input
// 'table' expecting <EOF>", so every design element after it in the same file
// is lost; row 12 carries file-scoped `timescale / `resetall directives, which
// would silently retime every scenario following them. Files are parsed
// independently, so neither can reach the shared fixture. Row 2 also raises
// syntax errors but the parser recovers locally (verified with a marker design
// element after it), so it stays in the shared fixture.
//
// The command file deliberately omits the "-timescale=1ns/1ns" flag that most
// tests in the Google suite carry: a global default time unit would mask every
// one of the Sec 3.14 scenarios, which are precisely about which design
// elements do and do not have a time unit of their own.
//
// Two shapes of test appear below, per .claude/instructions/davtests.md:
//   (a) HLC already diagnoses the violation -- assert it with the hlc::Test
//       base class's findError() overloads, which locate a diagnostic by
//       ErrorDefinition type plus the object it names (Location::m_object,
//       resolved through the SymbolTable) and/or its line:column. Never assert
//       a bare error count: this compilation carries many scenarios at once,
//       so the container totals are meaningless per scenario, and several
//       scenarios here share one ErrorType.
//   (b) HLC does not diagnose it yet -- the assertion the standard requires is
//       written out in full, with GTEST_SKIP() as the first statement citing
//       the clause. Eight of the twelve rows are in this state; that is the
//       point of the catalog. No test here asserts the absence of a
//       diagnostic, which would lock the gap in.
//
// Behaviour observed while writing this file, none of it diagnosed today and
// none of it asserted below (recorded here so the skip messages have context;
// time units are base-10 exponents of one second, 1ns == -9, 1ps == -12,
// 1us == -6):
//   - r9_m "timeunit 1ns / 1us" is stored as unit -9 / precision -6, i.e. a
//     precision coarser than the unit, without complaint (row 9).
//   - r10_m's second, mismatched "timeunit 1ps" silently overwrites the first
//     "timeunit 1ns" instead of being rejected (row 10).
//   - r13_m "timeunit 1step" is accepted and recorded as -12, i.e. "step" is
//     treated as if it meant 1ps (row 13).
//   - r12_b, declared after a `resetall, still inherits unit -9 / precision
//     -12 from the `timescale before it, so the mixed have/have-not condition
//     of 3.14.2.3 cannot even arise. That is a separate `resetall defect
//     (IEEE 1800-2023 22.3), noted in row 12's skip message.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

namespace hlc {

class Chapter3ErrorRulesTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "3--error_rules.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// --- row 2: illegal items inside a program block (3.4) ----------------------

TEST_F(Chapter3ErrorRulesTest, Row2_ProgramCannotContainModuleInstanceOrAlways) {
  // catalog row 2 | 3.4 | PARSE
  // A program block may contain only data declarations, class definitions,
  // subroutine definitions, object instances and initial/final procedures.
  // Both offending items sit in 3--error_rules.sv: the module instance on line
  // 103, the always procedure on line 104.
  EXPECT_NE(findError(ErrorDefinition::PA_SYNTAX_ERROR, 103, 2), nullptr)
      << "a module instance inside a program block is illegal (IEEE 1800-2023 3.4)";
  EXPECT_NE(findError(ErrorDefinition::PA_SYNTAX_ERROR, 104, 2), nullptr)
      << "an always procedure inside a program block is illegal (IEEE 1800-2023 3.4)";
}

// --- row 3: a primitive cannot instantiate a building block (3.11) ----------

TEST_F(Chapter3ErrorRulesTest, Row3_PrimitiveCannotInstantiateAnotherBuildingBlock) {
  // catalog row 3 | 3.11 | PARSE
  // Primitives are leaves in the hierarchy tree. The illegal instantiation is
  // on line 24 of 3--error_rules_inv2.sv.
  EXPECT_NE(findError(ErrorDefinition::PA_SYNTAX_ERROR, 24, 2), nullptr)
      << "a primitive cannot instantiate another building block (IEEE 1800-2023 3.11)";
}

// --- row 4: no forward references in a compilation unit (3.12.1) ------------

TEST_F(Chapter3ErrorRulesTest, Row4_ForwardReferenceInCompilationUnitIsRejected) {
  // catalog row 4 | 3.12.1 | COMP
  // "Other than for task and function names (see 23.8.1), references shall only
  // be made to names already defined in the compilation unit. The use of an
  // explicit $unit:: prefix only provides for name disambiguation and does not
  // add the ability to refer to later compilation-unit items."
  // Both offending references are in 3--error_rules.sv: the plain one on line
  // 23, the $unit::-prefixed one on line 24. 'r4_b' is declared on line 26.
  EXPECT_NE(findError(ErrorDefinition::COMP_FORWARD_REFERENCE_IN_COMPILATION_UNIT, "r4_b", 23, 11), nullptr)
      << "a forward reference to a compilation-unit variable must be diagnosed";
  EXPECT_NE(findError(ErrorDefinition::COMP_FORWARD_REFERENCE_IN_COMPILATION_UNIT, "r4_b", 24, 18), nullptr)
      << "the $unit:: prefix does not license a forward reference (IEEE 1800-2023 3.12.1)";
}

// --- rows 5-8: name spaces (3.13) -------------------------------------------

TEST_F(Chapter3ErrorRulesTest, Row5_DefinitionsNameSpaceReuseIsRejected) {
  // catalog row 5 | 3.13 | COMP
  // IEEE 1800-2023 3.13(a) makes the definitions name space shared across
  // module, primitive, program and interface definitions, so declaring
  // interface 'r5_dup' after module 'r5_dup' is illegal in any compilation
  // unit. Diagnosed in Phase2ModelBuilder::leaveDesign, which sees every
  // file's design elements because Phase 2 walks all files before it fires --
  // the same place row 6's package-name-space check already lives, which is
  // why the recorded category moved from LINT to COMP. Note the same severity
  // gap row 6 documents: COMP_MULTIPLY_DEFINED_DESIGN_UNIT is registered at
  // WARNING while 3.13 says "shall not"; this asserts the violation is
  // reported at all.
  EXPECT_NE(findError(ErrorDefinition::COMP_MULTIPLY_DEFINED_DESIGN_UNIT, "r5_dup"), nullptr)
      << "a name in the definitions name space cannot be reused (IEEE 1800-2023 3.13)";
}

TEST_F(Chapter3ErrorRulesTest, Row6_PackageNameSpaceReuseIsDiagnosed) {
  // catalog row 6 | 3.13 | LINT
  // This is the one name-space rule of the four that HLC does catch. Note the
  // severity gap: ErrorDefinition registers COMP_MULTIPLY_DEFINED_PACKAGE at
  // WARNING level, while 3.13 states the redeclaration is illegal. The
  // assertion here is that the violation is reported at all; the severity is
  // tracked separately in the catalog.
  EXPECT_NE(findError(ErrorDefinition::COMP_MULTIPLY_DEFINED_PACKAGE, "r6_p"), nullptr)
      << "a package name cannot be reused in the package name space (IEEE 1800-2023 3.13)";
}

TEST_F(Chapter3ErrorRulesTest, Row7_AttributeNameIsNotVisibleOutsideAttributeNameSpace) {
  // catalog row 7 | 3.13 | COMP
  // 3.13(h) keeps 'r7_fsm_state' out of the ordinary name space, which HLC
  // already did -- the reference never resolved to the Attribute. The error
  // itself comes from 6.10: a $display argument is not one of the three
  // positions that assume an implicit net, so the name is undeclared.
  EXPECT_NE(findError(ErrorDefinition::COMP_UNDEFINED_VARIABLE, "r7_fsm_state"), nullptr)
      << "an attribute name is not visible as an ordinary identifier (IEEE 1800-2023 3.13)";
}

TEST_F(Chapter3ErrorRulesTest, Row8_RedeclarationInSameNameSpaceIsRejected) {
  // catalog row 8 | 3.13 | COMP
  // Phase2 reuses an existing same-named Variable instead of creating a second
  // one, which is correct only for 3.13(g)'s port reintroduction; any other
  // redeclaration is now reported. Note the severity gap: the error code is
  // registered at WARNING while 3.13 says "shall be illegal".
  EXPECT_NE(findError(ErrorDefinition::COMP_MULTIPLY_DEFINED_VARIABLE, "a", 57, 7), nullptr)
      << "'a' is declared twice in the same name space (IEEE 1800-2023 3.13)";
}

// --- rows 9-13: time unit and time precision (3.14) -------------------------

TEST_F(Chapter3ErrorRulesTest, Row9_PrecisionCoarserThanUnitIsRejected) {
  // catalog row 9 | 3.14 | COMP
  // Time unit and precision are base-10 exponents of one second, so "at least as
  // precise" means precision <= unit; r9_m's 'timeunit 1ns / 1us' is -9 / -6.
  // Checked in Phase2's setDeclarationTimeInfo, which sees both values of the
  // declaration that states them.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_TIMESCALE, "r9_m"), nullptr)
      << "time precision cannot be coarser than the time unit (IEEE 1800-2023 3.14)";
}

TEST_F(Chapter3ErrorRulesTest, Row10_MismatchedRepeatedTimeunitIsRejected) {
  // catalog row 10 | 3.14.2.2 | COMP
  GTEST_SKIP() << "no diagnostic implemented; IEEE 1800-2023 3.14.2.2 allows at most one time unit "
                  "per design element and permits a repeated declaration only if it matches the "
                  "first -- r10_m declares 'timeunit 1ns' and later 'timeunit 1ps', and the second "
                  "silently overwrites the first";
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_TIMESCALE, "r10_m", 73, 3), nullptr)
      << "a repeated timeunit must match the first declaration (IEEE 1800-2023 3.14.2.2)";
}

TEST_F(Chapter3ErrorRulesTest, Row11_TimeunitAfterOtherItemsIsRejected) {
  // catalog row 11 | 3.14.2.2 | COMP
  GTEST_SKIP() << "no diagnostic implemented; IEEE 1800-2023 3.14.2.2 requires timeunit and "
                  "timeprecision to precede every other item in their time scope, and r11_m "
                  "declares 'logic a' before its 'timeunit 1ns'";
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_TIMESCALE, "r11_m", 80, 3), nullptr)
      << "timeunit must precede all other items in the time scope (IEEE 1800-2023 3.14.2.2)";
}

TEST_F(Chapter3ErrorRulesTest, Row12_MixedTimescaleAndNoTimescaleElementsAreRejected) {
  // catalog row 12 | 3.14.2.3 | LINT
  GTEST_SKIP() << "no diagnostic implemented; IEEE 1800-2023 3.14.2.3 makes it an error for some "
                  "design elements to have a time unit and precision while others do not. Two "
                  "defects stack here: no such whole-design check exists, and r12_b -- declared "
                  "after a `resetall -- still inherits its timescale from the preceding "
                  "`timescale, contrary to IEEE 1800-2023 22.3, so the mixed condition is not even "
                  "represented in the model. There is no dedicated ErrorDefinition code for this "
                  "rule yet, so the assertion below names the nearest existing one";
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_TIMESCALE, "r12_b"), nullptr)
      << "either all design elements carry a timescale or none do (IEEE 1800-2023 3.14.2.3)";
}

TEST_F(Chapter3ErrorRulesTest, Row13_StepCannotSetTheTimeUnit) {
  // catalog row 13 | 3.14.3 | PARSE
  GTEST_SKIP() << "no diagnostic implemented; IEEE 1800-2023 3.14.3 states that, unlike other time "
                  "units, 'step' cannot be used to set or modify the time unit or the time "
                  "precision -- r13_m's 'timeunit 1step' is accepted and treated as if it read 1ps";
  EXPECT_NE(findError(ErrorDefinition::PA_SYNTAX_ERROR, 87, 3), nullptr)
      << "'step' cannot set the time unit or precision (IEEE 1800-2023 3.14.3)";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
