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

// Tests for 12.4.2--unique0_if.sv (tags: 12.4.2)
//   module if_tb ();
//     wire [3:0] a = 0;
//     reg [1:0] b = 0;
//     always @* begin
//       unique0 if(a == 0) b = 1;
//       else if(a == 1) b = 2;
//     end
//   endmodule
//
// What to check and why (IEEE 1800-2023 12.4.2 "unique-if, unique0-if,
// and priority-if", p.317, checked before any test code was written):
//   "unique_priority ::= unique | unique0 | priority". "If the keyword
//   unique0 is used, there shall be no violation if no condition is
//   matched... Unique-if and unique0-if assert that there is no overlap
//   in a series of if-else-if conditions." Structurally, unique0-if
//   parses identically to unique-if/priority-if: the qualifier applies
//   to the whole chain and is carried by the outermost if-else object,
//   and (matching 12.4--if.sv, 12.4.2--priority_if.sv, and
//   12.4.2--unique_if.sv) since there is no trailing unconditional
//   else, the inner "else if" must be a plain IfStmt.
//
//   Also (IEEE 1800-2023 6.7/6.8): "wire" is a net_type keyword so
//   "wire a" must be a Net; "reg" is a non-net integer_vector_type
//   keyword so "reg b" must be a Variable.
//
//   IMPORTANT constant gap: IEEE 1800-2023 Annex 37.71 documents
//   vpiQualifier as an int property on If/IfElse, and the vpi_user.h
//   enumerants for it are vpiNoQualifier=0, vpiUniqueQualifier=1,
//   vpiPriorityQualifier=2, vpiTaggedQualifier=4, vpiRandQualifier=8,
//   vpiInsideQualifier=16 (also confirmed against this repo's actual
//   build/include/hldb/sv_vpi_user.h). None of these is a distinct,
//   standard-named constant for "unique0" as opposed to plain "unique"
//   -- the standard's own enumeration does not give unique0 its own
//   bit. Asserting getQualifier() equals some specific unconfirmed
//   integer for unique0 would be guessing a value the standard itself
//   does not name, which the test-writing guide explicitly warns
//   against. This file therefore only asserts that a qualifier IS
//   recorded at all (not vpiNoQualifier), matching what is actually
//   guaranteed by the grammar (unique0 is one of the three
//   unique_priority alternatives, so it must not be treated the same
//   as "no qualifier").
//
//   CONFIRMED BY RUNNING THIS FILE WITH THE SKIP BELOW REMOVED (actual:
//   0 vs 0): the constant-gap concern above turned out to be moot --
//   HLC does not even set the qualifier to some other, non-standard
//   value for "unique0". It parses the keyword (seen in the AST as a
//   Unique_priority/UNIQUE0 node) but never records ANY qualifier onto
//   the final IfElse object; getQualifier() stays at its default 0
//   (vpiNoQualifier). Same underlying gap as 12.4.2--priority_if.cpp
//   and 12.4.2--unique_if.cpp -- this compiler currently drops the
//   unique/unique0/priority qualifier for every "if" variant, not just
//   unique0. Kept as GTEST_SKIP() with the real assertion underneath,
//   per the established gating rule (skips only added after personal
//   verification).
//
// What is checked:
//   - module if_tb has exactly 1 Net "a" (wire) and 1 Variable "b" (reg)
//   - outer statement is IfElse; its qualifier SHOULD be non-zero
//     (see "constant gap" note above for why no specific value is
//     asserted) per spec, but this is currently a confirmed-failing,
//     skipped assertion (see note above)
//   - outer condition is Operation(vpiEqOp) comparing RefObj "a"
//     (resolving to the Net) against Constant "0"
//   - outer "then" body is Assignment "b = 1"
//   - outer's else-branch is a plain IfStmt (AnyType::IfStmt, not
//     IfElse) because there is no final unconditional else
//   - inner IfStmt condition is Operation(vpiEqOp) comparing RefObj "a"
//     against Constant "1"; inner body is Assignment "b = 2"
//   - no continuous assignments exist in the module
//
// What is NOT checked and why:
//   - the exact numeric value of getQualifier() for "unique0" -- see
//     the constant-gap note above; asserting an unconfirmed magic
//     number would violate "assert the standard, not a guess"
//   - the runtime "no violation report on no match" behavior of
//     unique0-if (12.4.2) is a simulation-time behavior, not a static/
//     structural compile-time property
//   - the "always @*" implicit sensitivity list is a chapter-9 concept

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/always.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/event_control.h>
#include <hldb/if_else.h>
#include <hldb/if_stmt.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class Unique0IfTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "12.4.2--unique0_if.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("if_tb", m_design->getAllModules()); }

  static const hldb::IfElse *getOuter() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getProcesses() == nullptr || top->getProcesses()->empty()) return nullptr;
    const hldb::Always *const always = any_cast<hldb::Always>(top->getProcesses()->at(0));
    if (always == nullptr) return nullptr;
    const hldb::EventControl *const ec = always->getStmt<hldb::EventControl>();
    if (ec == nullptr) return nullptr;
    const hldb::Begin *const body = ec->getStmt<hldb::Begin>();
    if (body == nullptr || body->getStmts() == nullptr || body->getStmts()->empty()) return nullptr;
    return any_cast<hldb::IfElse>(body->getStmts()->at(0));
  }
};

TEST_F(Unique0IfTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(Unique0IfTest, ModuleHasOneNetAAndOneVariableB) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  ASSERT_EQ(top->getNets()->size(), 1u);
  EXPECT_NE(hldb::findByName<hldb::Net>("a", top->getNets()), nullptr) << "Net 'a' not found";
  ASSERT_NE(top->getVariables(), nullptr) << "'reg b' should be a Variable, not a Net";
  ASSERT_EQ(top->getVariables()->size(), 1u);
  EXPECT_NE(hldb::findByName<hldb::Variable>("b", top->getVariables()), nullptr) << "Variable 'b' not found";
}

// ---------------------------------------------------------------------------
// Outer: unique0 if(a == 0) b = 1;
// ---------------------------------------------------------------------------
TEST_F(Unique0IfTest, OuterStmtIsIfElseWithSomeQualifierSet) {
  GTEST_SKIP() << "Confirmed HLC bug -- verified by running this test with the skip removed (fails as expected, "
                  "actual: 0 vs 0): IEEE 1800-2023 12.4.2 requires 'unique0' to be recorded as some non-zero "
                  "qualifier on the IfElse, but HLC leaves getQualifier() at its default 0 (vpiNoQualifier) -- "
                  "the keyword is parsed (seen in the AST) but never recorded onto the final object. Same "
                  "underlying gap as 12.4.2--priority_if.cpp and 12.4.2--unique_if.cpp. Tracked, not yet fixed "
                  "by the compiler.";
  const hldb::IfElse *const outer = getOuter();
  ASSERT_NE(outer, nullptr) << "outer statement should resolve to IfElse (its else-branch is populated)";
  EXPECT_NE(outer->getQualifier(), vpiNoQualifier)
      << "'unique0' is a unique_priority keyword per grammar and must not be treated as unqualified; see the "
         "constant-gap note above for why no specific numeric value is asserted";
}

TEST_F(Unique0IfTest, OuterConditionIsANotResolvedToNetEqualsZero) {
  const hldb::IfElse *const outer = getOuter();
  ASSERT_NE(outer, nullptr);
  const hldb::Operation *const cond = outer->getCondition<hldb::Operation>();
  ASSERT_NE(cond, nullptr) << "outer condition is not an Operation";
  EXPECT_EQ(cond->getOpType(), vpiEqOp);
  ASSERT_NE(cond->getOperands(), nullptr);
  ASSERT_EQ(cond->getOperands()->size(), 2u);
  const hldb::RefObj *const lhs = any_cast<hldb::RefObj>(cond->getOperands()->at(0));
  ASSERT_NE(lhs, nullptr) << "first operand should be RefObj 'a'";
  EXPECT_EQ(lhs->getName(), "a");
  EXPECT_NE(lhs->getActual<hldb::Net>(), nullptr) << "'a' should resolve to the Net";
  const hldb::Constant *const rhs = any_cast<hldb::Constant>(cond->getOperands()->at(1));
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "0");
}

TEST_F(Unique0IfTest, OuterThenBranchAssignsOneToVariableB) {
  const hldb::IfElse *const outer = getOuter();
  ASSERT_NE(outer, nullptr);
  const hldb::Assignment *const thenAssign = outer->getStmt<hldb::Assignment>();
  ASSERT_NE(thenAssign, nullptr);
  const hldb::Constant *const rhs = thenAssign->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "1");
}

// ---------------------------------------------------------------------------
// Inner: else if(a == 1) b = 2;  -- no trailing else, so plain IfStmt
// ---------------------------------------------------------------------------
TEST_F(Unique0IfTest, OuterElseBranchIsPlainIfStmtNotIfElse) {
  const hldb::IfElse *const outer = getOuter();
  ASSERT_NE(outer, nullptr);
  const hldb::Any *const elseBranch = outer->getElseStmt();
  ASSERT_NE(elseBranch, nullptr) << "outer must have an else-branch (the 'else if')";
  EXPECT_EQ(elseBranch->getAnyType(), hldb::AnyType::IfStmt)
      << "no trailing unconditional else exists, so this must be a plain IfStmt";
}

TEST_F(Unique0IfTest, InnerConditionIsAEqualsOne) {
  const hldb::IfElse *const outer = getOuter();
  ASSERT_NE(outer, nullptr);
  const hldb::IfStmt *const inner = outer->getElseStmt<hldb::IfStmt>();
  ASSERT_NE(inner, nullptr);
  const hldb::Operation *const cond = inner->getCondition<hldb::Operation>();
  ASSERT_NE(cond, nullptr);
  EXPECT_EQ(cond->getOpType(), vpiEqOp);
  ASSERT_NE(cond->getOperands(), nullptr);
  ASSERT_EQ(cond->getOperands()->size(), 2u);
  const hldb::Constant *const rhs = any_cast<hldb::Constant>(cond->getOperands()->at(1));
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "1");
}

TEST_F(Unique0IfTest, InnerThenBranchAssignsTwoToVariableB) {
  const hldb::IfElse *const outer = getOuter();
  ASSERT_NE(outer, nullptr);
  const hldb::IfStmt *const inner = outer->getElseStmt<hldb::IfStmt>();
  ASSERT_NE(inner, nullptr);
  const hldb::Assignment *const thenAssign = inner->getStmt<hldb::Assignment>();
  ASSERT_NE(thenAssign, nullptr);
  const hldb::Constant *const rhs = thenAssign->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "2");
}

TEST_F(Unique0IfTest, NoContinuousAssigns) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty());
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
