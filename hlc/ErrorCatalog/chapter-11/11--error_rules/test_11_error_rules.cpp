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
// scenarios catalogued in sv_error_catalog_Latest.xlsx / docs/error_catalog.xml
// (rows 333, 338, 340, 341, 343, 346, 362, 363, 368, 369, 373, 375).
//
// Scope, fixture layout and test shapes follow the Clause 3 file in this same
// suite; see hlc/ErrorCatalog/chapter-3 for the rationale.
//
// Behaviour observed while writing this file (hlc.exe -d db over the fixture):
// the four original modules all compile. Row 375's let-in-a-class is rejected
// by the grammar (a let is not a class_item), which is why it is placed last
// in the fixture -- the recovery cascade costs four syntax errors on line 115
// alone and would otherwise take later scenarios with it. Rows 362, 363, 368
// and 373 produce no diagnostic at all.
//
// Rows 333, 338, 340, 341, 343, 346 and 369 were merged in from a sibling
// snapshot of this same catalog file; several of them (338, 340, 369) are not
// yet implemented in HLC's Linter and are marked GTEST_SKIP() accordingly --
// see each test's own comment.

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

// --- row 333: assignment operator outside a legal expression context (11.3.6) ---

TEST_F(Chapter11ErrorRulesTest, Row333_AssignmentOperatorOutsideProceduralContextIsRejected) {
  // catalog row 333 | 11.3.6 | COMP
  // "It shall be illegal to include an assignment operator in an event
  // expression, in an expression within a procedural continuous
  // assignment, or in an expression that is not within a procedural
  // statement." All three forms appear in r333_m: 'always @((a = b))',
  // 'assign w = (a += 1)', and 'initial assign a = (b = 2)'.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_EXPRESSION_CONTEXT, "r333_m"), nullptr)
      << "an assignment operator is illegal in an event expression, a procedural continuous "
         "assignment's own expression, or any expression outside a procedural statement "
         "(IEEE 1800-2023 11.3.6)";
}

// --- row 338: select of a concatenation cannot be an lvalue (11.4.12) -------

TEST_F(Chapter11ErrorRulesTest, Row338_SelectOfConcatenationCannotBeAssignmentLhs) {
  // catalog row 338 | 11.4.12 | COMP
  GTEST_SKIP() << "not yet implemented in HLC's Linter (IEEE 1800-2023 11.4.12)";
  // "A bit-select or part-select applied to a concatenation shall not be
  // legal as a net_lvalue or variable_lvalue, i.e. it shall not appear on
  // the left-hand side of an assignment." r338_m's '{a, b}[3:0] = 4'h5'
  // applies a part-select to a concatenation as an assignment target.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_ASSIGNMENT_LHS, "r338_m"), nullptr)
      << "a bit-select or part-select of a concatenation is not a legal net_lvalue or "
         "variable_lvalue (IEEE 1800-2023 11.4.12)";
}

// --- row 340: a replication cannot be an lvalue (11.4.12.1) -----------------

TEST_F(Chapter11ErrorRulesTest, Row340_ReplicationCannotBeAssignmentLhs) {
  // catalog row 340 | 11.4.12.1 | COMP
  GTEST_SKIP() << "not yet implemented in HLC's Linter (IEEE 1800-2023 11.4.12.1)";
  // "An expression containing a replication shall not appear on the
  // left-hand side of an assignment." r340_m's '{4{a}} = 4'b1010' uses a
  // replication as the assignment target.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_ASSIGNMENT_LHS, "r340_m"), nullptr)
      << "an expression containing a replication shall not appear on the left-hand side of an "
         "assignment (IEEE 1800-2023 11.4.12.1)";
}

// --- row 341: a replication cannot connect to an output/inout port (11.4.12.1) ---

TEST_F(Chapter11ErrorRulesTest, Row341_ReplicationCannotConnectToOutputPort) {
  // catalog row 341 | 11.4.12.1 | LINT
  GTEST_SKIP() << "not yet implemented in HLC's Linter (IEEE 1800-2023 11.4.12.1)";
  // "An expression containing a replication shall not be connected to an
  // output or inout port." r341_m's 'r341_sub u1({4{a}})' connects a
  // replication to r341_sub's output port 'o'.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_EXPRESSION_CONTEXT, "r341_m"), nullptr)
      << "an expression containing a replication shall not be connected to an output or inout "
         "port (IEEE 1800-2023 11.4.12.1)";
}

// --- row 343: string concatenation cannot be an lvalue (11.4.12.2) ----------

TEST_F(Chapter11ErrorRulesTest, Row343_StringConcatenationCannotBeAssignmentLhs) {
  // catalog row 343 | 11.4.12.2 | COMP
  // "String concatenation is allowed only as an expression; it shall not
  // appear on the left-hand side of an assignment." r343_m's
  // '{a, b} = "hello"' uses a string concatenation as the assignment
  // target.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_ASSIGNMENT_LHS, "r343_m"), nullptr)
      << "string concatenation is expression-only and shall not appear on the left-hand side "
         "of an assignment (IEEE 1800-2023 11.4.12.2)";
}

// --- row 346: a streaming concatenation cannot be a bare operand (11.4.14) ---

TEST_F(Chapter11ErrorRulesTest, Row346_StreamingConcatenationCannotBeBareOperand) {
  // catalog row 346 | 11.4.14 | COMP
  GTEST_SKIP() << "not yet implemented in HLC's Linter (IEEE 1800-2023 11.4.14)";
  // "It shall be an error to use a streaming_concatenation as an operand in
  // an expression without first casting it to a bit-stream type." r346_m's
  // 'r = {>>{a}} + b' uses a streaming concatenation directly as an
  // operand of '+'.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_EXPRESSION_CONTEXT, "r346_m"), nullptr)
      << "a streaming_concatenation used as an operand must first be cast to a bit-stream type "
         "(IEEE 1800-2023 11.4.14)";
}

// --- row 362: a tagged union expression needs a context (11.9) --------------

TEST_F(Chapter11ErrorRulesTest, Row362_ContextFreeTaggedUnionExpressionIsRejected) {
  // catalog row 362 | 11.9 | COMP
  // The type of a tagged union expression is not carried by the expression
  // itself; it shall be known from its context -- an assignment target, a
  // cast, or an enclosing expression whose type is known. A $display argument
  // supplies none of those, so line 69 is illegal.
  EXPECT_NE(findError(ErrorDefinition::COMP_UNKNOWN_EXPRESSION_TYPE, "Valid", 69, 20), nullptr)
      << "a tagged union expression with no type context is illegal (IEEE 1800-2023 11.9)";
}

// --- row 363: only real member names may follow 'tagged' (11.9) -------------

TEST_F(Chapter11ErrorRulesTest, Row363_UnknownTaggedUnionMemberIsRejected) {
  // catalog row 363 | 11.9 | COMP
  // "The only member names allowed after the tagged keyword are the member
  // names of the tagged union type of the expression." r363_vint declares
  // Invalid and Valid; line 78 names Bogus.
  EXPECT_NE(findError(ErrorDefinition::COMP_UNDEFINED_MEMBER, "Bogus", 78, 22), nullptr)
      << "Bogus is not a member of the tagged union type (IEEE 1800-2023 11.9)";
}

// --- row 368: typed let formals are restricted (11.12) ----------------------

TEST_F(Chapter11ErrorRulesTest, Row368_IllegalLetFormalArgumentTypeIsRejected) {
  // catalog row 368 | 11.12 | COMP
  // "If a formal argument of a let is typed, then the type shall be event or
  // one of the types allowed in 16.6." 16.6 requires a type cast compatible
  // with an integral type and bans chandle by name; 6.22.5 makes class handles
  // type incompatible with everything. Line 85 declares a chandle formal.
  //
  // This case was a real formal until 2026-09-04. That was wrong: 16.6's
  // criterion is cast compatibility, and 6.22.4 counts "all nonequivalent types
  // that have defined explicit casting rules", which real satisfies via 6.24 --
  // so a real let formal is legal and asserting an error on it would have
  // locked in a rejection of valid SystemVerilog.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_FORMAL_TYPE, "x", 85, 9), nullptr)
      << "chandle is not a legal let formal argument type (IEEE 1800-2023 11.12)";
}

// --- row 369: an event-typed let formal must stay in event-expression context (11.12) ---

TEST_F(Chapter11ErrorRulesTest, Row369_EventTypedLetFormalReferencedOutsideEventExpressionContext) {
  // catalog row 369 | 11.12 | COMP
  GTEST_SKIP() << "not yet implemented in HLC's Linter (IEEE 1800-2023 11.12)";
  // "If a let formal argument is of type event, the actual argument shall
  // be an event_expression and every reference to that formal shall
  // appear where an event_expression may be written." r369_m declares
  // 'let r369_e(event ev) = x + 1' and then calls it as
  // 'x = r369_e(posedge clk)', a plain assignment context rather than
  // one where an event_expression may be written.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_EXPRESSION_CONTEXT, "r369_m"), nullptr)
      << "an event-typed let formal's call must appear only where an event_expression may be "
         "written (IEEE 1800-2023 11.12)";
}

// --- row 373: lets may not recurse (11.12) ----------------------------------

TEST_F(Chapter11ErrorRulesTest, Row373_RecursiveLetIsRejected) {
  // catalog row 373 | 11.12 | COMP
  // "Recursive let instantiations are not permitted." The body of r on line 103
  // instantiates r.
  EXPECT_NE(findError(ErrorDefinition::COMP_RECURSIVE_DEFINITION, "r", 103, 14), nullptr)
      << "a let cannot instantiate itself (IEEE 1800-2023 11.12)";
}

// --- row 375: where a let may be declared (11.12) ---------------------------

TEST_F(Chapter11ErrorRulesTest, Row375_LetDeclaredInAClassBodyIsRejected) {
  // catalog row 375 | 11.12 | COMP
  // A let declaration is permitted in a module, interface, program, checker,
  // clocking block, package, compilation-unit scope, generate block,
  // sequential or parallel block, or subroutine. A class body is not on that
  // list, so the declaration on line 115 is illegal.
  //
  // HLC rejects it in the grammar: let is not one of the class_item
  // alternatives, so the parser reports "extraneous input 'let'" and then
  // three more errors while it recovers across the rest of the declaration.
  // Asserting only the first keeps the test tied to the construct rather than
  // to the shape of ANTLR's recovery.
  EXPECT_NE(findError(ErrorDefinition::PA_SYNTAX_ERROR, 115, 2), nullptr)
      << "a let may not be declared in a class body (IEEE 1800-2023 11.12)";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
