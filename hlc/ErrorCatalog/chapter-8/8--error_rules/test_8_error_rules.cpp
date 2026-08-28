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

// Tests for the IEEE 1800-2023 Clause 8 (Classes) error scenarios catalogued
// in sv_error_catalog_Latest.xlsx (rows 198, 201, 203, 207, 215, 230, 239).
//
// Scope: this file asserts ONLY that the diagnostic each catalog row requires
// is emitted. It makes no assertion about the shape of the compiled model.
// Exactly one TEST_F per catalog row, named Row<N>_... after that row and
// carrying a "catalog row N | clause | category" comment; that is the link
// between the workbook and this file.
//
// One fixture, 8--error_rules.sv, holds every scenario, so no test here may
// assert an error count -- the container totals are meaningless per scenario.
// Every assertion goes through the hlc::Test base class's findError()
// overloads, which locate a diagnostic by ErrorDefinition type plus the object
// it names and/or its line:column.
//
// Two shapes of test appear below, per .claude/instructions/davtests.md:
//   (a) HLC already diagnoses the violation -- assert it live.
//   (b) HLC does not diagnose it yet -- the assertion the standard requires is
//       written out in full, with GTEST_SKIP() as the first statement citing
//       the clause. No test asserts the ABSENCE of a diagnostic, which would
//       lock the gap in.
//
// Behaviour observed while writing this file (hlc.exe -d db over the fixture),
// recorded so the skip messages have context:
//   - row 198 is diagnosed: COMP_ILLEGAL_TIMING_CONTROL_IN_FUNCTION fires at
//     21:5 naming "new". This is the one Clause 8 row already wired.
//   - row 207 is rejected by the GRAMMAR, not by a semantic check: HLC accepts
//     super.new only as the first statement of a constructor, so the misplaced
//     call on line 57 is a syntax error. A control fixture with super.new
//     correctly placed first parses cleanly, which is what makes this
//     positional enforcement rather than a missing production.
//   - rows 201, 203, 215, 230 and 239 produce no diagnostic naming the rule.
//     Row 230 does raise an unrelated "Failed to bind new" warning plus a
//     Linter null-actual error at 82:15; that is a binding gap for the new
//     call itself, not the abstract-class rule, so it is not asserted here.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

namespace hlc {

class Chapter8ErrorRulesTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "8--error_rules.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// --- row 198: a constructor may not block (8.7) -----------------------------

TEST_F(Chapter8ErrorRulesTest, Row198_TimingControlInConstructorIsRejected) {
  // catalog row 198 | 8.7 | COMP
  // "The class constructor new is defined as a function with no return type
  // and, like any other function, it shall be nonblocking." 13.4(a) spells out
  // what that forbids: a function shall not contain any time-controlling
  // statement. The #10 is on line 21 of 8--error_rules.sv.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_TIMING_CONTROL_IN_FUNCTION, "new", 21, 5), nullptr)
      << "a delay control inside a constructor is illegal (IEEE 1800-2023 8.7, 13.4)";
}

// --- row 201: static methods cannot touch non-static members (8.10) ---------

TEST_F(Chapter8ErrorRulesTest, Row201_NonStaticAccessFromStaticMethodIsRejected) {
  // catalog row 201 | 8.10 | COMP
  // "A static method has no access to non-static members ... but it can
  // directly access static class properties or call static methods of the same
  // class. Access to non-static members or to the special this handle within
  // the body of a static method is illegal and results in a compiler error."
  // Two violations in r201_c: the bare property on line 32, this on line 33.
  GTEST_SKIP() << "no diagnostic implemented; IEEE 1800-2023 8.10 makes a reference to a non-static "
                  "class property, and any use of 'this', illegal inside a static method";
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_NONSTATIC_ACCESS, "nonstat", 32, 5), nullptr)
      << "a static method cannot access a non-static property (IEEE 1800-2023 8.10)";
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_NONSTATIC_ACCESS, "nonstat", 33, 5), nullptr)
      << "'this' is not available inside a static method (IEEE 1800-2023 8.10)";
}

// --- row 203: 'this' outside a class context (8.11) -------------------------

TEST_F(Chapter8ErrorRulesTest, Row203_ThisOutsideAClassIsRejected) {
  // catalog row 203 | 8.11 | COMP
  // "The this keyword shall only be used as type(this) or within non-static
  // class methods, constraints, inlined constraint methods, or covergroups
  // embedded within classes; otherwise an error shall be issued." The initial
  // block of r203_m on line 43 is module scope, none of those.
  //
  // The check reports only the "no class context at all" case: all five
  // permitted contexts sit inside a class, so a ClassDefn ancestor is what
  // separates them. 'this' inside a STATIC class method is therefore NOT
  // covered here -- it has a class around it and belongs to 8.10 (row 201).
  // The symbol is the enclosing named scope, per the catalog's "%s = where
  // this appeared".
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_THIS, "r203_m", 43, 15), nullptr)
      << "'this' cannot be used outside a class context (IEEE 1800-2023 8.11)";
}

// --- row 207: super.new must come first (8.15) ------------------------------

TEST_F(Chapter8ErrorRulesTest, Row207_SuperNewMustBeTheFirstStatement) {
  // catalog row 207 | 8.15 | COMP
  // "super.new shall be the first statement executed in the constructor."
  // r207_b assigns x before calling it, on line 57.
  //
  // HLC enforces this in the grammar rather than semantically: super.new is a
  // production of its own that is only accepted at the head of a constructor
  // body, so the misplaced call is a syntax error. That was verified against a
  // control fixture in which super.new IS first -- it parses cleanly -- so the
  // rejection is positional, not a missing production. The rule is therefore
  // already honoured, and the catalog's COMP_MISPLACED_SUPER_NEW code appears
  // to be unnecessary; recorded here rather than acted on.
  EXPECT_NE(findError(ErrorDefinition::PA_SYNTAX_ERROR, 57, 10), nullptr)
      << "super.new must be the first statement of the constructor (IEEE 1800-2023 8.15)";
}

// --- row 215: extends-specifier args and super.new are exclusive (8.17) -----

TEST_F(Chapter8ErrorRulesTest, Row215_SuperNewWithArgumentsInExtendsIsRejected) {
  // catalog row 215 | 8.17 | COMP
  // "If the superclass constructor arguments are specified in the extends
  // specifier, then the subclass constructor shall not contain a super.new()
  // call." r215_ether names them both ways: (5) in the extends specifier on
  // line 68 and super.new(5) on line 70.
  //
  // Note the contrast with row 207: this super.new IS the first statement of
  // its constructor, so the grammar accepts it and nothing else objects.
  GTEST_SKIP() << "no diagnostic implemented; IEEE 1800-2023 8.17 forbids a super.new call when the "
                  "superclass constructor arguments are already given in the extends specifier";
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_SUPER_NEW, "r215_ether", 70, 5), nullptr)
      << "super.new is illegal when the extends specifier already supplies the arguments "
         "(IEEE 1800-2023 8.17)";
}

// --- row 230: abstract classes cannot be constructed (8.21) -----------------

TEST_F(Chapter8ErrorRulesTest, Row230_AbstractClassCannotBeConstructedDirectly) {
  // catalog row 230 | 8.21 | COMP
  // "An abstract class ... shall not be directly instantiated. Its constructor
  // can only be called ... by the chaining of constructor calls originating in
  // an extended non-abstract object." r230_base is declared virtual on line 78
  // and constructed directly on line 82.
  //
  // HLC does emit two diagnostics at 82:15 -- a CP5851 "Failed to bind new"
  // warning and a Linter null-actual error -- but both are about the unbound
  // new call, not about the class being abstract, and they fire for
  // non-abstract classes too. Neither is asserted here.
  GTEST_SKIP() << "no diagnostic implemented; IEEE 1800-2023 8.21 forbids constructing an object of "
                  "an abstract (virtual) class directly";
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_CONSTRUCTION, "r230_base", 82, 15), nullptr)
      << "an object of an abstract class cannot be constructed (IEEE 1800-2023 8.21)";
}

// --- row 239: out-of-block declarations follow the class (8.24) -------------

TEST_F(Chapter8ErrorRulesTest, Row239_OutOfBlockDeclarationMustFollowTheClass) {
  // catalog row 239 | 8.24 | COMP
  // "The out-of-block declaration shall be declared in the same scope as the
  // class declaration and shall follow the class declaration." The body on
  // line 92 precedes class r239_c, which starts on line 94.
  GTEST_SKIP() << "no diagnostic implemented; IEEE 1800-2023 8.24 requires an out-of-block method "
                  "declaration to follow the class declaration in the same scope";
  EXPECT_NE(findError(ErrorDefinition::COMP_MISPLACED_EXTERN_DECLARATION, "r239_c", 92, 1), nullptr)
      << "an out-of-block declaration must follow its class (IEEE 1800-2023 8.24)";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
