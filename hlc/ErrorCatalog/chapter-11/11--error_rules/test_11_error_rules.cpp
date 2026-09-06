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

// Tests for the IEEE 1800-2023 Clause 11 (Operators and expressions) error
// scenarios catalogued in sv_error_catalog_Latest.xlsx (rows 362, 363, 368,
// 373, 375).
//
// Scope, fixture layout and test shapes follow the Clause 3 file in this same
// suite; see hlc/ErrorCatalog/chapter-3 for the rationale.
//
// Behaviour observed while writing this file (hlc.exe -d db over the fixture):
// the four modules all compile. Row 375's let-in-a-class is rejected by the
// grammar (a let is not a class_item), which is why it is placed last in the
// fixture -- the recovery cascade costs four syntax errors on line 48 alone
// and would otherwise take later scenarios with it. Rows 362, 363, 368 and 373
// produce no diagnostic at all.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

namespace hlc {

class Chapter11ErrorRulesTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "11--error_rules.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// --- row 362: a tagged union expression needs a context (11.9) --------------

TEST_F(Chapter11ErrorRulesTest, Row362_ContextFreeTaggedUnionExpressionIsRejected) {
  // catalog row 362 | 11.9 | COMP
  // The type of a tagged union expression is not carried by the expression
  // itself; it shall be known from its context -- an assignment target, a
  // cast, or an enclosing expression whose type is known. A $display argument
  // supplies none of those, so line 13 is illegal.
  EXPECT_NE(findError(ErrorDefinition::COMP_UNKNOWN_EXPRESSION_TYPE, "Valid", 13, 20), nullptr)
      << "a tagged union expression with no type context is illegal (IEEE 1800-2023 11.9)";
}

// --- row 363: only real member names may follow 'tagged' (11.9) -------------

TEST_F(Chapter11ErrorRulesTest, Row363_UnknownTaggedUnionMemberIsRejected) {
  // catalog row 363 | 11.9 | COMP
  // "The only member names allowed after the tagged keyword are the member
  // names of the tagged union type of the expression." r363_vint declares
  // Invalid and Valid; line 22 names Bogus.
  EXPECT_NE(findError(ErrorDefinition::COMP_UNDEFINED_MEMBER, "Bogus", 22, 22), nullptr)
      << "Bogus is not a member of the tagged union type (IEEE 1800-2023 11.9)";
}

// --- row 368: typed let formals are restricted (11.12) ----------------------

TEST_F(Chapter11ErrorRulesTest, Row368_IllegalLetFormalArgumentTypeIsRejected) {
  // catalog row 368 | 11.12 | COMP
  // "If a formal argument of a let is typed, then the type shall be event or
  // one of the types allowed in 16.6." 16.6 requires a type cast compatible
  // with an integral type and bans chandle by name; 6.22.5 makes class handles
  // type incompatible with everything. Line 29 declares a chandle formal.
  //
  // This case was a real formal until 2026-09-04. That was wrong: 16.6's
  // criterion is cast compatibility, and 6.22.4 counts "all nonequivalent types
  // that have defined explicit casting rules", which real satisfies via 6.24 --
  // so a real let formal is legal and asserting an error on it would have
  // locked in a rejection of valid SystemVerilog.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_FORMAL_TYPE, "x", 29, 9), nullptr)
      << "chandle is not a legal let formal argument type (IEEE 1800-2023 11.12)";
}

// --- row 373: lets may not recurse (11.12) ----------------------------------

TEST_F(Chapter11ErrorRulesTest, Row373_RecursiveLetIsRejected) {
  // catalog row 373 | 11.12 | COMP
  // "Recursive let instantiations are not permitted." The body of r on line 36
  // instantiates r.
  EXPECT_NE(findError(ErrorDefinition::COMP_RECURSIVE_DEFINITION, "r", 36, 14), nullptr)
      << "a let cannot instantiate itself (IEEE 1800-2023 11.12)";
}

// --- row 375: where a let may be declared (11.12) ---------------------------

TEST_F(Chapter11ErrorRulesTest, Row375_LetDeclaredInAClassBodyIsRejected) {
  // catalog row 375 | 11.12 | COMP
  // A let declaration is permitted in a module, interface, program, checker,
  // clocking block, package, compilation-unit scope, generate block,
  // sequential or parallel block, or subroutine. A class body is not on that
  // list, so the declaration on line 48 is illegal.
  //
  // HLC rejects it in the grammar: let is not one of the class_item
  // alternatives, so the parser reports "extraneous input 'let'" and then
  // three more errors while it recovers across the rest of the declaration.
  // Asserting only the first keeps the test tied to the construct rather than
  // to the shape of ANTLR's recovery.
  EXPECT_NE(findError(ErrorDefinition::PA_SYNTAX_ERROR, 48, 2), nullptr)
      << "a let may not be declared in a class body (IEEE 1800-2023 11.12)";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
