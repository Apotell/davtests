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

// Tests for the IEEE 1800-2023 Clause 10 (Assignment statements) error
// scenarios catalogued in sv_error_catalog_Latest.xlsx (rows 290, 298, 302,
// 303, 306, 309, 316, 317, 322, 323).
//
// Scope, fixture layout and test shapes follow the Clause 3 file in this same
// suite; see hlc/ErrorCatalog/chapter-3 for the rationale.
//
// Behaviour observed while writing this file (hlc.exe -d db over the fixture):
// row 290's and row 309's modules compile with neither rule diagnosed. Row
// 309's assignment pattern in a port expression does not even raise a syntax
// error -- the parser accepts the header and the port .p is silently given an
// implicit wire type (CP5810 at 72:18), after which the Linter reports an
// orphan node and two unnamed RefTypespecs. Those are downstream damage from
// an illegal construct that was never rejected, not the required diagnostic,
// so none of them is asserted here. Row 306's keyed assignment-pattern-as-LHS
// statement ('{x:a, y:b} = s;) is rejected by HLC's own grammar with a
// PA_SYNTAX_ERROR instead of the semantic diagnostic the rule calls for; that
// parse failure recovers locally -- every module declared after r306_m in the
// fixture still compiles -- so it stays in this shared file rather than
// moving to a sibling _inv*.sv.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

namespace hlc {

class Chapter10ErrorRulesTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "10--error_rules.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// --- row 290: continuous assignment left-hand side (10.2) -------------------

TEST_F(Chapter10ErrorRulesTest, Row290_NonConstantSelectOnContinuousAssignLhsIsRejected) {
  // catalog row 290 | 10.2 | COMP
  // Table 10-1 enumerates what may appear on the left-hand side of a
  // continuous assignment: a net or variable, a CONSTANT bit-select or
  // CONSTANT part-select of a vector net or packed variable, or a
  // concatenation of those. r290_m indexes w with the variable idx on line 15,
  // which is none of them.
  GTEST_SKIP() << "no diagnostic implemented; IEEE 1800-2023 10.2 Table 10-1 allows only a constant "
                  "bit-select or part-select on the left-hand side of a continuous assignment";
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_ASSIGNMENT_LHS, "w", 15, 10), nullptr)
      << "a non-constant bit-select is illegal on the LHS of a continuous assignment "
         "(IEEE 1800-2023 10.2)";
}

// --- row 298: atomic net (user-defined nettype) select on LHS (10.3.2) ------

TEST_F(Chapter10ErrorRulesTest, Row298_SelectIntoAtomicNetLhsIsRejected) {
  // catalog row 298 | 10.3.2 | COMP
  // A continuous assignment to an atomic net (a net of a user-defined
  // nettype) shall not drive part of the net; the entire nettype value shall
  // be driven. r298_m's line 27, "assign n[0] = 1'b1;", indexes into the
  // nettype's data type on the LHS.
  GTEST_SKIP() << "no diagnostic implemented; IEEE 1800-2023 10.2 Table 10-1 allows only a constant "
                  "bit-select or part-select on the left-hand side of a continuous assignment";
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_ASSIGNMENT_LHS, "r298_m", 27, 10), nullptr)
      << "a continuous assignment to an atomic (user-defined nettype) net shall not "
         "select part of it (IEEE 1800-2023 10.3.2)";
}

// --- row 302: bit-/part-select LHS of a procedural continuous assign (10.6.1)

TEST_F(Chapter10ErrorRulesTest, Row302_SelectOnProceduralContinuousAssignLhsIsRejected) {
  // catalog row 302 | 10.6.1 | COMP
  // The left-hand side of the assignment in an "assign" procedural
  // continuous assignment statement shall be a singular variable reference
  // or a concatenation of variables; it shall not be a bit-select or a
  // part-select of a variable. r302_m has both forms: line 37's "assign
  // q[3] = ..." (bit-select) and line 38's "assign q[3:0] = ..."
  // (part-select).
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_ASSIGNMENT_LHS, "r302_m", 37, 11), nullptr)
      << "the LHS of a procedural continuous assign shall not be a bit-select or "
         "part-select of a variable (IEEE 1800-2023 10.6.1)";
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_ASSIGNMENT_LHS, "r302_m", 38, 11), nullptr)
      << "the LHS of a procedural continuous assign shall not be a bit-select or "
         "part-select of a variable (IEEE 1800-2023 10.6.1)";
}

// --- row 303: select LHS of a force/release (10.6.2) ------------------------

TEST_F(Chapter10ErrorRulesTest, Row303_SelectOnForceReleaseLhsIsRejected) {
  // catalog row 303 | 10.6.2 | COMP
  // The left-hand side of a force or release shall be a singular variable, a
  // net, a constant bit-select of a vector net, a constant part-select of a
  // vector net, or a concatenation of these; it shall not be a bit-select or
  // part-select of a variable or of a net with a user-defined nettype.
  // r303_m has both forms: line 52's "force v[0] = ..." (a bit-select of a
  // variable) and line 53's "force n[0] = ..." (a select of a user-defined
  // nettype net).
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_ASSIGNMENT_LHS, "r303_m", 52, 5), nullptr)
      << "the LHS of force/release shall not be a select of a variable or of a "
         "user-defined nettype net (IEEE 1800-2023 10.6.2)";
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_ASSIGNMENT_LHS, "r303_m", 53, 5), nullptr)
      << "the LHS of force/release shall not be a select of a variable or of a "
         "user-defined nettype net (IEEE 1800-2023 10.6.2)";
}

// --- row 306: keyed notation on the LHS of an assignment pattern (10.9) ----

TEST_F(Chapter10ErrorRulesTest, Row306_KeyedNotationOnAssignmentPatternLhsIsRejected) {
  // catalog row 306 | 10.9 | COMP
  // When an assignment pattern is used as the left-hand side of an
  // assignment-like context, positional notation shall be required;
  // keyed/named or default notation is illegal there. r306_m's line 66,
  // "'{x:a, y:b} = s;", uses keyed notation on the LHS. HLC's own grammar
  // currently rejects this construct outright with a PA_SYNTAX_ERROR, rather
  // than accepting it and diagnosing the illegal notation semantically as
  // the standard's own rule requires.
  GTEST_SKIP() << "HLC's grammar rejects this construct with a syntax error instead of the semantic "
                  "diagnostic IEEE 1800-2023 10.9 requires for keyed notation on an assignment "
                  "pattern LHS";
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_ASSIGNMENT_LHS, "x", 66, 13), nullptr)
      << "keyed/named notation is illegal on the LHS of an assignment pattern "
         "(IEEE 1800-2023 10.9)";
}

// --- row 309: assignment patterns in port expressions (10.9) ----------------

TEST_F(Chapter10ErrorRulesTest, Row309_AssignmentPatternInPortExpressionIsRejected) {
  // catalog row 309 | 10.9 | COMP
  // "An assignment pattern expression shall not be used in a port expression
  // in a module, interface, or program declaration." r309_m's header on line
  // 72 uses one.
  //
  // HLC accepts the header outright: no syntax error, and the port is given an
  // implicit wire type as if the pattern were an ordinary port expression. The
  // Linter's later orphan-node and unnamed-RefTypespec complaints are
  // consequences of that acceptance, not the rule, and are not asserted.
  //
  // NOT IMPLEMENTED, deliberately: the model cannot answer the question. A port
  // expression in a DECLARATION keeps only a bare reference -- verified with
  // three headers: .p(int'{1}) leaves the Port with no high-connection at all,
  // while .p({a, b}) and .p(a[1:0]) both reduce to vpiHighConn RefObj "a",
  // dropping the concatenation and the part-select. So nothing on the Port
  // records that a pattern was written there. Assignment patterns also have no
  // object of their own anywhere in the model: at an instantiation site,
  // .q(s_t'{1'b0, 1'b1}) surfaces as a generic Operation. Both would have to
  // change before this rule is decidable after binding.
  GTEST_SKIP() << "no diagnostic implemented; IEEE 1800-2023 10.9 forbids an assignment pattern "
                  "expression in a port expression of a module, interface or program declaration";
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_EXPRESSION_CONTEXT, "p", 72, 18), nullptr)
      << "an assignment pattern cannot be a port expression (IEEE 1800-2023 10.9)";
}

// --- row 316: unpacked array concat outside an assignment-like context (10.10)

TEST_F(Chapter10ErrorRulesTest, Row316_UnpackedArrayConcatOutsideAssignmentLikeContextIsRejected) {
  // catalog row 316 | 10.10 | COMP
  // An unpacked array concatenation may appear only as the source expression
  // in an assignment-like context and shall not appear in any other context.
  // r316_m's line 80, "$display({a, b} == 0);", uses the concatenation as an
  // operand of "==", not as an assignment's source expression.
  GTEST_SKIP() << "no diagnostic implemented; IEEE 1800-2023 10.10 forbids an unpacked array "
                  "concatenation from appearing outside an assignment-like context";
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_EXPRESSION_CONTEXT, "a", 80, 21), nullptr)
      << "an unpacked array concatenation may appear only as the source expression "
         "of an assignment-like context (IEEE 1800-2023 10.10)";
}

// --- row 317: associative array as the target of an unpacked array concat (10.10)

TEST_F(Chapter10ErrorRulesTest, Row317_AssociativeArrayTargetOfUnpackedArrayConcatIsRejected) {
  // catalog row 317 | 10.10 | LINT
  // The target of an unpacked array concatenation shall be an array whose
  // slowest-varying dimension is an unpacked fixed-size, queue, or dynamic
  // dimension; a target of any other type, including an associative array,
  // shall be illegal. r317_m's line 91, "assoc = {a, b};", targets an
  // associative array.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_ASSIGNMENT_LHS, "r317_m", 91, 11), nullptr)
      << "an associative array shall not be the target of an unpacked array "
         "concatenation (IEEE 1800-2023 10.10)";
}

// --- row 322: replication syntax inside an unpacked array concat (10.10.1) --

TEST_F(Chapter10ErrorRulesTest, Row322_ReplicationInsideUnpackedArrayConcatIsRejected) {
  // catalog row 322 | 10.10.1 | COMP
  // Unpacked array concatenations forbid replication, defaulting, and
  // explicit typing of the concatenation itself. r322_m's line 100, "A9 =
  // {9{1}};", uses replication syntax as the unpacked array concatenation
  // assigned to a fixed-size unpacked array.
  GTEST_SKIP() << "no diagnostic implemented; IEEE 1800-2023 10.10.1 forbids replication syntax "
                  "inside an unpacked array concatenation";
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_EXPRESSION_CONTEXT, "A9", 100, 11), nullptr)
      << "replication syntax is illegal inside an unpacked array concatenation "
         "(IEEE 1800-2023 10.10.1)";
}

// --- row 323: an unpacked array concat nested inside another one (10.10.3) --

TEST_F(Chapter10ErrorRulesTest, Row323_NestedUnpackedArrayConcatIsRejected) {
  // catalog row 323 | 10.10.3 | COMP
  // Because a complete unpacked array concatenation has no self-determined
  // type, it shall be illegal for an unpacked array concatenation to appear
  // as an item in another unpacked array concatenation. r323_m's line 111,
  // "SQ = {S1, {S2, S1}};", nests one unpacked array concatenation inside
  // another.
  GTEST_SKIP() << "no diagnostic implemented; IEEE 1800-2023 10.10.3 forbids an unpacked array "
                  "concatenation from appearing nested inside another one";
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_EXPRESSION_CONTEXT, "SQ", 111, 11), nullptr)
      << "an unpacked array concatenation shall not appear as an item of another "
         "one (IEEE 1800-2023 10.10.3)";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
