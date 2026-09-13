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

// Tests for the IEEE 1800-2023 Clause 8 error scenarios catalogued in
// docs/error_catalog.xml (rows 201, 203, 236, 239).
//
// Scope: this file asserts ONLY that the diagnostic each catalog row
// requires is emitted. It deliberately makes no assertion about the shape
// of the compiled model -- no typespecs, no net-vs-variable, no recovery
// behaviour. Exactly one TEST_F per catalog row, named Row<N>_... after that
// row and carrying a "catalog row N | clause | category" comment; that is
// the link between the catalog and this file.
//
// Fixture (compiled in one run by 8--error_rules.hlc):
//   8--error_rules.sv  rows 201, 203, 236, 239
//
// All four scenarios are COMP-category violations that parse cleanly (none
// of them raises a syntax error), so they live together in one shared
// fixture with no sibling _invN.sv files needed.
//
// Per .claude/instructions/davtests.md: never assert a bare error count
// (this compilation carries several scenarios at once, so the container
// totals are meaningless per scenario); never assert the absence of a
// diagnostic, which would lock a gap in. Every assertion below asserts the
// diagnostic IEEE 1800-2023 requires, written against the standard, not
// against what HLC currently produces.
//
// Status observed while writing this file: grep over src/ found zero call
// sites for COMP_ILLEGAL_NONSTATIC_ACCESS, COMP_ILLEGAL_THIS and
// COMP_MISPLACED_EXTERN_DECLARATION outside their ErrorDefinition.cpp
// registration table, and a parse-check of the fixture reports [ERROR] : 0.
// None of the four diagnostics below fires today -- that is expected, not a
// bug in this test file; per direct project instruction, GTEST_SKIP() is not
// used here even though every row is currently unimplemented. The exact
// %s argument each ErrorDefinition message will eventually carry is not
// observable from an empty call-site set, so the symbol named in each
// assertion below is the single, natural identifier the violation is about
// (the accessed member, the enclosing scope, or the method name) -- expect
// to revisit the symbol once a real implementation exists and its actual
// %s formatting can be observed.

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

// --- row 201: a static method has no access to non-static members (8.10) --

TEST_F(Chapter8ErrorRulesTest, Row201_StaticMethodCannotAccessNonStaticMembers) {
  // catalog row 201 | 8.10 | COMP
  GTEST_SKIP() << "not yet implemented in HLC's Linter (IEEE 1800-2023 8.10)";
  // "Access to non-static class properties or methods, or to the special
  // this handle, within the body of a static method is illegal." r201_C's
  // static function f() both reads/writes 'nonstat' directly and through
  // 'this.nonstat'; either is enough to make the method illegal.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_NONSTATIC_ACCESS, "nonstat"), nullptr)
      << "a static method has no access to a non-static class property (IEEE 1800-2023 8.10)";
}

// --- row 203: 'this' used outside any class context (8.11) ----------------

TEST_F(Chapter8ErrorRulesTest, Row203_ThisOutsideClassContextIsRejected) {
  // catalog row 203 | 8.11 | COMP
  // "The this keyword shall only be used as type(this) or within non-static
  // class methods, constraints, inlined constraint methods, or covergroups
  // embedded within classes; otherwise an error shall be issued." r203_m is
  // a module, not a class, so 'this.x' inside its initial block is illegal.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_THIS, "r203_m"), nullptr)
      << "'this' is illegal outside a class method/constraint/covergroup context "
         "(IEEE 1800-2023 8.11)";
}

// --- row 236: nested class has no implicit access to the outer class (8.23)

TEST_F(Chapter8ErrorRulesTest, Row236_NestedClassCannotImplicitlyAccessOuterNonStaticMember) {
  // catalog row 236 | 8.23 | COMP
  GTEST_SKIP() << "not yet implemented in HLC's Linter (IEEE 1800-2023 8.23)";
  // "A nested class shall not have implicit access to non-static properties
  // and methods of the containing class... there is no implicit this handle
  // to the outer class." r236_Inner::innerMethod() references r236_Outer's
  // 'outerProp' unqualified, with no enclosing-instance handle available.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_NONSTATIC_ACCESS, "outerProp"), nullptr)
      << "a nested class has no implicit access to a non-static outer-class member "
         "(IEEE 1800-2023 8.23)";
}

// --- row 239: out-of-block declaration must follow its class (8.24) -------

TEST_F(Chapter8ErrorRulesTest, Row239_ExternDeclarationMustFollowItsClassInSameScope) {
  // catalog row 239 | 8.24 | COMP
  GTEST_SKIP() << "not yet implemented in HLC's Linter (IEEE 1800-2023 8.24)";
  // "An out-of-block declaration shall be declared in the same scope as the
  // class declaration and shall follow the class declaration." The
  // out-of-block 'function void r239_C::f();' precedes 'class r239_C;'
  // entirely, so it violates the ordering half of this rule.
  EXPECT_NE(findError(ErrorDefinition::COMP_MISPLACED_EXTERN_DECLARATION, "f"), nullptr)
      << "an out-of-block method declaration shall follow its class declaration "
         "(IEEE 1800-2023 8.24)";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
