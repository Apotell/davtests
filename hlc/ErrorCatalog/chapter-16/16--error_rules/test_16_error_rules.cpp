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

// Tests for the IEEE 1800-2023 Clause 16 (Assertions) error scenarios
// catalogued in sv_error_catalog_Latest.xlsx / docs/error_catalog.xml (rows
// 468, 474, 476, 477, 478, 483, 484, 489, 490, 496, 511, 512, 513, 514, 515,
// 544, 554, 560, 563).
//
// Scope, fixture layout and test shapes follow the Clause 3 file in this same
// suite; see hlc/ErrorCatalog/chapter-3 for the rationale.
//
// Behaviour observed while writing this file (hlc.exe -d db over the fixture):
// all three original modules (468, 476, 515) compile with no syntax errors.
// The only diagnostic is a CP5811 "Port b definition missing its direction"
// warning at 141:39, which is about how r515_m's header is written and has
// nothing to do with any of these rules; it is not asserted.
//
// Rows 496 and 544 were added later and are both grammar-level rejections
// (PA_SYNTAX_ERROR), not gaps in HLC's semantic checking -- see each test's
// own comment.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

namespace hlc {

class Chapter16ErrorRulesTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "16--error_rules.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// --- row 468: deferred assertion action blocks (16.4) -----------------------

TEST_F(Chapter16ErrorRulesTest, Row468_BeginEndActionBlockOnDeferredAssertionIsRejected) {
  // catalog row 468 | 16.4 | COMP
  // For a deferred immediate assertion, "the pass and fail statements ... shall
  // each consist of a single subroutine call." A begin-end block is not a
  // subroutine call, so the else branch starting on line 13 is illegal. Note
  // this restriction is specific to the deferred form (assert #0); the same
  // block on a plain immediate assertion would be legal.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_ACTION_BLOCK, "r468_a1", 13, 38), nullptr)
      << "a deferred assertion action block must be a single subroutine call "
         "(IEEE 1800-2023 16.4)";
}

// --- row 474: non-static class property referenced in a concurrent
//              assertion (16.6) --------------------------------------------

TEST_F(Chapter16ErrorRulesTest, Row474_NonStaticClassPropertyInAssertionIsRejected) {
  // catalog row 474 | 16.6 | LINT
  GTEST_SKIP() << "not yet implemented in HLC's Linter (IEEE 1800-2023 16.6)";
  // Expressions in concurrent assertions shall not reference non-static
  // class properties or methods; "obj.p" is a non-static property of
  // r474_c.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_NONSTATIC_ACCESS, "r474_m"), nullptr)
      << "a concurrent assertion cannot reference a non-static class property (IEEE 1800-2023 16.6)";
}

// --- row 476: side effect (++/--) directly in an assertion expression
//              (16.6) -------------------------------------------------------

TEST_F(Chapter16ErrorRulesTest, Row476_SideEffectInAssertionExpressionIsRejected) {
  // catalog row 476 | 16.6 | COMP
  // "Evaluation of an expression ... shall not have any side effects, e.g.,
  // increment and decrement operators are not allowed." The sole exemption is
  // a sequence match item whose variable_lvalue is a local variable; cnt on
  // line 38 is a module-level int in the property expression itself, not a
  // match item, so the exemption does not reach it.
  GTEST_SKIP() << "no diagnostic implemented; IEEE 1800-2023 16.6 forbids side effects in a "
                  "concurrent assertion expression, exempting only sequence match items whose "
                  "lvalue is a local variable";
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_SIDE_EFFECT, "cnt", 38, 44), nullptr)
      << "an increment operator is a side effect and is illegal in an assertion expression "
         "(IEEE 1800-2023 16.6)";
}

// --- row 477: function with an output argument in a concurrent
//              assertion (16.6) --------------------------------------------

TEST_F(Chapter16ErrorRulesTest, Row477_FunctionWithOutputArgInAssertionIsRejected) {
  // catalog row 477 | 16.6 | LINT
  // Functions that appear in concurrent assertion expressions shall not
  // contain output, inout, or ref arguments (const ref is allowed); "f(o)"
  // calls a function whose formal 'o' is declared output.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_EXPRESSION_CONTEXT, "r477_m"), nullptr)
      << "a function with an output argument cannot be called from a concurrent assertion "
         "expression (IEEE 1800-2023 16.6)";
}

// --- row 478: non-side-effect-free function in a concurrent
//              assertion (16.6) --------------------------------------------

TEST_F(Chapter16ErrorRulesTest, Row478_FunctionWithStateOrSideEffectsInAssertionIsRejected) {
  // catalog row 478 | 16.6 | LINT
  GTEST_SKIP() << "not yet implemented in HLC's Linter (IEEE 1800-2023 16.6)";
  // Functions appearing in concurrent assertion expressions shall be
  // automatic (or preserve no state information) and have no side effects;
  // 'f' preserves state in its static 'calls' variable.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_SIDE_EFFECT, "f"), nullptr)
      << "a function called from a concurrent assertion must be side-effect free "
         "(IEEE 1800-2023 16.6)";
}

// --- row 483: named sequence instance missing an actual for a
//              default-less formal (16.8) -----------------------------------

TEST_F(Chapter16ErrorRulesTest, Row483_NamedSequenceInstanceMissingActualForFormalIsRejected) {
  // catalog row 483 | 16.8 | COMP
  GTEST_SKIP() << "not yet implemented in HLC's Linter (IEEE 1800-2023 16.8)";
  // An instance of a named sequence shall provide an actual argument for
  // each formal argument that does not have a default actual argument
  // declared; "r483_s(a)" omits the actual for formal 'y'.
  EXPECT_NE(findError(ErrorDefinition::COMP_MISSING_ARGUMENT, "y"), nullptr)
      << "a named sequence instance must supply an actual for every formal without a default "
         "(IEEE 1800-2023 16.8)";
}

// --- row 484: $ as a named-sequence actual bound to a formal used as an
//              ordinary operand (16.8) --------------------------------------

TEST_F(Chapter16ErrorRulesTest, Row484_DollarActualBoundToFormalUsedAsOrdinaryOperandIsRejected) {
  // catalog row 484 | 16.8 | LINT
  GTEST_SKIP() << "not yet implemented in HLC's Linter (IEEE 1800-2023 16.8)";
  // If the terminal $ is an actual argument of a named sequence instance,
  // the corresponding formal argument's every reference shall either be a
  // cycle_delay_const_range_expression upper bound or itself an actual of
  // a named sequence instance; 'r484_s(a, $)' binds $ to 'y', which is used
  // as an ordinary operand of ##1.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_EXPRESSION_CONTEXT, "r484_m"), nullptr)
      << "$ can only be bound to a formal whose every reference is a range upper bound or a "
         "named-sequence actual (IEEE 1800-2023 16.8)";
}

// --- row 489: sequence-typed formal used as the operand of goto
//              repetition (16.8.1) -------------------------------------------

TEST_F(Chapter16ErrorRulesTest, Row489_SequenceTypedFormalUsedAsGotoRepetitionOperandIsRejected) {
  // catalog row 489 | 16.8.1 | LINT
  GTEST_SKIP() << "not yet implemented in HLC's Linter (IEEE 1800-2023 16.8.1)";
  // If a formal argument is of type sequence, each reference to it shall
  // be in a place where a sequence_expr is legal or as an operand of the
  // sequence methods triggered or matched; "q[->2]" uses the sequence
  // formal 'q' as the operand of a goto repetition, which is neither.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_EXPRESSION_CONTEXT, "r489_m"), nullptr)
      << "a sequence-typed formal cannot be used as a goto-repetition operand "
         "(IEEE 1800-2023 16.8.1)";
}

// --- row 490: event-typed formal instance not given an event_expression
//              actual (16.8.1) -----------------------------------------------

TEST_F(Chapter16ErrorRulesTest, Row490_EventTypedFormalActualNotAnEventExpressionIsRejected) {
  // catalog row 490 | 16.8.1 | LINT
  GTEST_SKIP() << "not yet implemented in HLC's Linter (IEEE 1800-2023 16.8.1)";
  // If a formal argument is of type event, the actual argument shall be an
  // event_expression; "r490_s(clk)" binds plain 'clk' rather than an
  // event_expression such as "posedge clk".
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_EXPRESSION_CONTEXT, "r490_m"), nullptr)
      << "an event-typed formal's actual argument must be an event_expression "
         "(IEEE 1800-2023 16.8.1)";
}

// --- row 511: global clocking sampled value function with no global
//              clocking declaration (16.9.4) ----------------------------------

TEST_F(Chapter16ErrorRulesTest, Row511_GlobalClockingSampledValueFunctionWithNoGlobalClockingIsRejected) {
  // catalog row 511 | 16.9.4 | ELAB
  // The global clocking past and future sampled value functions may be
  // used only if global clocking is defined by a global clocking
  // declaration; r511_m never declares one.
  EXPECT_NE(findError(ErrorDefinition::COMP_MISSING_DEFAULT_CLOCKING, "$rose_gclk"), nullptr)
      << "a global clocking sampled value function requires a global clocking declaration "
         "(IEEE 1800-2023 16.9.4)";
}

// --- row 512: global clocking future function in an assertion action
//              block (16.9.4) -------------------------------------------------

TEST_F(Chapter16ErrorRulesTest, Row512_FutureSampledValueFunctionInActionBlockIsRejected) {
  // catalog row 512 | 16.9.4 | LINT
  // The global clocking future sampled value functions may be invoked only
  // in a property_expr or a sequence_expr; "$future_gclk(sig)" appears in
  // the else clause's action block.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_EXPRESSION_CONTEXT, "r512_m"), nullptr)
      << "a future sampled value function cannot appear in an assertion action block "
         "(IEEE 1800-2023 16.9.4)";
}

// --- row 513: nested global clocking future functions (16.9.4) -------------

TEST_F(Chapter16ErrorRulesTest, Row513_NestedFutureSampledValueFunctionsAreRejected) {
  // catalog row 513 | 16.9.4 | COMP
  // The global clocking future sampled value functions shall not be
  // nested; "$future_gclk(a || $rising_gclk(b))" nests $rising_gclk inside
  // $future_gclk.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_EXPRESSION_CONTEXT, "r513_m"), nullptr)
      << "global clocking future sampled value functions cannot be nested (IEEE 1800-2023 16.9.4)";
}

// --- row 514: global clocking future function in an assertion with
//              sequence match items (16.9.4) ---------------------------------

TEST_F(Chapter16ErrorRulesTest, Row514_FutureSampledValueFunctionWithMatchItemsIsRejected) {
  // catalog row 514 | 16.9.4 | LINT
  GTEST_SKIP() << "not yet implemented in HLC's Linter (IEEE 1800-2023 16.9.4)";
  // The global clocking future sampled value functions shall not be used
  // in assertions containing sequence match items; sequence 'r514_s' has
  // the match item "v = a".
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_EXPRESSION_CONTEXT, "r514_m"), nullptr)
      << "a future sampled value function cannot be used in an assertion containing a sequence "
         "match item (IEEE 1800-2023 16.9.4)";
}

// --- row 515: assertion variable declaration types (16.10) ------------------

TEST_F(Chapter16ErrorRulesTest, Row515_IllegalAssertionVariableTypeIsRejected) {
  // catalog row 515 | 16.10 | COMP
  // "The data type of an assertion variable declaration shall be specified
  // explicitly and shall be one of the types allowed within assertions as
  // defined in 16.6." 16.6 admits the integral types and their arrays; chandle
  // is not among them, so the local declaration on line 143 is illegal.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_DECLARED_TYPE, "h", 143, 13), nullptr)
      << "chandle is not a legal assertion variable type (IEEE 1800-2023 16.10)";
}

// --- row 554: matched method used outside a sequence expression (16.13.6) --

TEST_F(Chapter16ErrorRulesTest, Row554_MatchedMethodUsedOutsideSequenceExpressionIsRejected) {
  // catalog row 554 | 16.13.6 | LINT
  // The matched method can only be used in sequence expressions;
  // "wait (r554_e1.matched);" uses it inside a procedural wait statement.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_EXPRESSION_CONTEXT, "r554_m"), nullptr)
      << "the matched method can only be used in a sequence expression (IEEE 1800-2023 16.13.6)";
}

// --- row 560: $inferred_clock not used as an entire formal default value
//              expression (16.14.7) --------------------------------------------

TEST_F(Chapter16ErrorRulesTest, Row560_InferredClockNotUsedAsEntireFormalDefaultValueIsRejected) {
  // catalog row 560 | 16.14.7 | COMP
  // An inferred clocking or disable function ($inferred_clock,
  // $inferred_disable) shall only be used as the entire default value
  // expression for a formal argument to a property, sequence, or checker
  // declaration; here "@($inferred_clock)" is used as a clocking event,
  // not as a formal's default value.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_EXPRESSION_CONTEXT, "r560_m"), nullptr)
      << "$inferred_clock may only be used as the entire default value expression of a "
         "property/sequence/checker formal (IEEE 1800-2023 16.14.7)";
}

// --- row 563: explicit clocking event inside a clocking block's sequence
//              declaration (16.16) --------------------------------------------

TEST_F(Chapter16ErrorRulesTest, Row563_ExplicitClockingEventInsideClockingBlockDeclarationIsRejected) {
  // catalog row 563 | 16.16 | COMP
  // No explicit clocking event is allowed in any property or sequence
  // declaration within a clocking block; sequence 'r563_s1' writes its own
  // "@(posedge clk)" even though it is declared inside 'r563_posedge_clk'.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_EXPRESSION_CONTEXT, "r563_m"), nullptr)
      << "a sequence declared inside a clocking block cannot carry its own explicit clocking "
         "event (IEEE 1800-2023 16.16)";
}

// --- row 496: a directioned sequence port item also needs local (16.8.2) ---

TEST_F(Chapter16ErrorRulesTest, Row496_DirectionedSequencePortItemNeedsLocalKeyword) {
  // catalog row 496 | 16.8.2 | COMP
  // r496_s's "sequence r496_s(output logic v);" on line 188 gives formal 'v'
  // a direction without the local keyword.
  //
  // sequence_port_item's own grammar already makes this unwritable: a
  // direction can only ever follow LOCAL ("(LOCAL sequence_lvar_port_
  // direction?)? sequence_formal_type identifier"), so "output" with no
  // preceding "local" is rejected outright as a syntax error at 188:18
  // ("extraneous input 'output'") -- the same shape as row 573's grammar-
  // level rejection in hlc/ErrorCatalog/chapter-17. The rule is honoured
  // either way, which is what this asserts; the catalog's own
  // COMP_ILLEGAL_QUALIFIER code never gets a chance to run here.
  EXPECT_NE(findError(ErrorDefinition::PA_SYNTAX_ERROR, 188, 18), nullptr)
      << "a direction on a sequence port item requires the local keyword (IEEE 1800-2023 16.8.2)";
}

// --- row 544: a property's local formal may only be input (16.12.19) -------

TEST_F(Chapter16ErrorRulesTest, Row544_PropertyLocalFormalMustBeInputDirection) {
  // catalog row 544 | 16.12.19 | COMP
  // r544_p's "property r544_p(local output int lv);" on line 200 declares a
  // local formal with direction output.
  //
  // Same grammar-level rejection as row 496 above: property_port_item's own
  // grammar only ever allows INPUT as a property_lvar_port_direction
  // ("(LOCAL property_lvar_port_direction?)? property_formal_type
  // identifier", and property_lvar_port_direction: INPUT alone), so "output"
  // is rejected outright as a syntax error at 200:24 ("extraneous input
  // 'output'") before the catalog's own COMP_ILLEGAL_QUALIFIER code would
  // ever run.
  EXPECT_NE(findError(ErrorDefinition::PA_SYNTAX_ERROR, 200, 24), nullptr)
      << "a property's local formal argument cannot have direction output (IEEE 1800-2023 16.12.19)";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
