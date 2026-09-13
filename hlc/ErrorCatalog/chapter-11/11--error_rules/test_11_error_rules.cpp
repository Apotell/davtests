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

// Tests for the IEEE 1800-2023 Clause 11 error scenarios catalogued in
// docs/error_catalog.xml (rows 333, 338, 340, 341, 343, 346, 369).
//
// Scope: this file asserts ONLY that the diagnostic each catalog row
// requires is emitted. It deliberately makes no assertion about the shape
// of the compiled model -- no typespecs, no net-vs-variable, no recovery
// behaviour. Exactly one TEST_F per catalog row, named Row<N>_... after
// that row and carrying a "catalog row N | clause | category" comment;
// that is the link between the catalog and this file.
//
// Fixture: 11--error_rules.sv holds all seven scenarios. Every scenario in
// this chapter is Category COMP or LINT (an expression-context / lvalue
// rule, not PA_SYNTAX_ERROR per the catalog), so no scenario needed a
// sibling _invK.sv fixture the way chapter 3 does. In practice, three of
// the seven scenarios (rows 338, 340, 369) currently DO trip a real
// PA_SYNTAX_ERROR in HLC's grammar before any semantic check could run --
// {a,b}[3:0], {4{a}} and a let-with-event-formal call are all rejected as
// syntax today -- but recovery is local (every module in the fixture still
// gets its own "Compile module" pass, no "expecting <EOF>" cascade), so
// they stay in the shared fixture. This is not asserted below: the tests
// assert the catalog row's own designated ErrorDefinition code (what the
// standard's rule maps to), not the incidental PA_SYNTAX_ERROR that
// happens to fire today -- asserting the latter would lock in what may
// well be its own, separate defect (the grammar rejecting syntax the
// standard treats as a semantic-only illegality) rather than exercising
// the rule this file is actually cataloguing.
//
// None of the two ErrorDefinition codes this chapter's rows map to
// (COMP_ILLEGAL_ASSIGNMENT_LHS, COMP_ILLEGAL_EXPRESSION_CONTEXT) has a
// single call site anywhere in src/ (confirmed by grep) -- both exist only
// as table entries in ErrorDefinition.cpp. So every test below is expected
// to fail (red) today; that is the point of this catalog, not a bug to
// work around. No test asserts the absence of a diagnostic, which would
// lock the gap in.

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
  // "It shall be an error to use a streaming_concatenation as an operand in
  // an expression without first casting it to a bit-stream type." r346_m's
  // 'r = {>>{a}} + b' uses a streaming concatenation directly as an
  // operand of '+'.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_EXPRESSION_CONTEXT, "r346_m"), nullptr)
      << "a streaming_concatenation used as an operand must first be cast to a bit-stream type "
         "(IEEE 1800-2023 11.4.14)";
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

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
