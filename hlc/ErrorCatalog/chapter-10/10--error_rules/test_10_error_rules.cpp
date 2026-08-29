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
// scenarios catalogued in sv_error_catalog_Latest.xlsx (rows 290, 309).
//
// Scope, fixture layout and test shapes follow the Clause 3 file in this same
// suite; see hlc/ErrorCatalog/chapter-3 for the rationale.
//
// Behaviour observed while writing this file (hlc.exe -d db over the fixture):
// both modules compile. Neither rule is diagnosed. Row 309's assignment
// pattern in a port expression does not even raise a syntax error -- the
// parser accepts the header and the port .p is silently given an implicit wire
// type (CP5810 at 25:18), after which the Linter reports an orphan node and
// two unnamed RefTypespecs. Those are downstream damage from an illegal
// construct that was never rejected, not the required diagnostic, so none of
// them is asserted here.

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

// --- row 309: assignment patterns in port expressions (10.9) ----------------

TEST_F(Chapter10ErrorRulesTest, Row309_AssignmentPatternInPortExpressionIsRejected) {
  // catalog row 309 | 10.9 | COMP
  // "An assignment pattern expression shall not be used in a port expression
  // in a module, interface, or program declaration." r309_m's header on line
  // 25 uses one.
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
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_EXPRESSION_CONTEXT, "p", 25, 18), nullptr)
      << "an assignment pattern cannot be a port expression (IEEE 1800-2023 10.9)";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
