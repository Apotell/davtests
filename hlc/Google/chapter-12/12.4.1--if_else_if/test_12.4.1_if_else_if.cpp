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

// Tests for 12.4.1--if_else_if.sv (tags: 12.4.1)
//   module if_tb ();
//     wire a = 0;
//     reg b = 0;
//     wire c = 0;
//     reg d = 0;
//     always @* begin
//       if(a) b = 1;
//       else if(c) d = 1;
//       else b = 0;
//     end
//   endmodule
//
// What to check and why (IEEE 1800-2023 12.4.1 "if-else-if construct",
// p.316, checked before any test code was written):
//   "This sequence of if-else statements (known as an if-else-if
//   construct) is the most general way of writing a multiway decision.
//   The expressions shall be evaluated in order. If any expression is
//   true, the statement associated with it shall be executed... The
//   last else of the if-else-if construct handles the none-of-the-above
//   ... case." This file chains exactly one "else if" followed by a
//   final unconditional "else", so the object model should be a nested
//   IfElse: the outer IfElse's else-branch is itself another IfElse
//   (not a plain IfStmt, since that inner if also has its own else),
//   and that inner IfElse's else-branch is finally a plain Assignment.
//
//   Also (IEEE 1800-2023 6.7/6.8): "wire" is a net_type keyword so
//   "wire a"/"wire c" must be Nets; "reg" is a non-net
//   integer_vector_type keyword so "reg b"/"reg d" must be Variables.
//
// What is checked:
//   - module if_tb has exactly 2 Nets ("a", "c") and 2 Variables
//     ("b", "d")
//   - outer statement is IfElse, qualifier vpiNoQualifier, condition
//     RefObj "a" resolving to Net "a", "then" body Assignment "b = 1"
//   - outer IfElse's else-branch is itself an IfElse (AnyType::IfElse),
//     not a plain Assignment and not a plain IfStmt -- proving the
//     "else if" was chained rather than terminating the construct
//   - inner IfElse: qualifier vpiNoQualifier, condition RefObj "c"
//     resolving to Net "c", "then" body Assignment "d = 1"
//   - inner IfElse's else-branch is Assignment "b = 0" (the final
//     unconditional else), present directly (not wrapped in Begin)
//   - no continuous assignments exist in the module
//
// What is NOT checked and why:
//   - the "always @*" implicit sensitivity list is a chapter-9 concept,
//     out of scope here
//   - runtime execution/simulation of which branch fires is a
//     simulation-time concept, not a static/structural property

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
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/ref_obj.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class IfElseIfTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "12.4.1--if_else_if.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("if_tb", m_design->getAllModules()); }

  static const hldb::IfElse *getOuterIfElse() {
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

TEST_F(IfElseIfTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

// ---------------------------------------------------------------------------
// Declarations -- wire 'a','c' are Nets, reg 'b','d' are Variables
// ---------------------------------------------------------------------------
TEST_F(IfElseIfTest, ModuleHasTwoNetsAndTwoVariables) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  ASSERT_EQ(top->getNets()->size(), 2u);
  EXPECT_NE(hldb::findByName<hldb::Net>("a", top->getNets()), nullptr) << "Net 'a' not found";
  EXPECT_NE(hldb::findByName<hldb::Net>("c", top->getNets()), nullptr) << "Net 'c' not found";
  ASSERT_NE(top->getVariables(), nullptr) << "'reg' declarations should be Variables, not Nets";
  ASSERT_EQ(top->getVariables()->size(), 2u);
  EXPECT_NE(hldb::findByName<hldb::Variable>("b", top->getVariables()), nullptr) << "Variable 'b' not found";
  EXPECT_NE(hldb::findByName<hldb::Variable>("d", top->getVariables()), nullptr) << "Variable 'd' not found";
}

// ---------------------------------------------------------------------------
// Outer if(a) b = 1; else if(c) d = 1; else b = 0;
// ---------------------------------------------------------------------------
TEST_F(IfElseIfTest, OuterStmtIsIfElseWithNoQualifier) {
  const hldb::IfElse *const outer = getOuterIfElse();
  ASSERT_NE(outer, nullptr) << "the outer statement should resolve to IfElse";
  EXPECT_EQ(outer->getQualifier(), vpiNoQualifier);
}

TEST_F(IfElseIfTest, OuterConditionResolvesToNetANotVariable) {
  const hldb::IfElse *const outer = getOuterIfElse();
  ASSERT_NE(outer, nullptr);
  const hldb::RefObj *const cond = outer->getCondition<hldb::RefObj>();
  ASSERT_NE(cond, nullptr);
  EXPECT_EQ(cond->getName(), "a");
  EXPECT_NE(cond->getActual<hldb::Net>(), nullptr) << "'a' should resolve to the Net, not a Variable";
}

TEST_F(IfElseIfTest, OuterThenBranchAssignsOneToVariableB) {
  const hldb::IfElse *const outer = getOuterIfElse();
  ASSERT_NE(outer, nullptr);
  const hldb::Assignment *const thenAssign = outer->getStmt<hldb::Assignment>();
  ASSERT_NE(thenAssign, nullptr);
  const hldb::RefObj *const lhs = thenAssign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "b");
  EXPECT_NE(lhs->getActual<hldb::Variable>(), nullptr);
  const hldb::Constant *const rhs = thenAssign->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "1");
}

// ---------------------------------------------------------------------------
// The chained "else if" -- outer's else-branch must be another IfElse
// ---------------------------------------------------------------------------
TEST_F(IfElseIfTest, OuterElseBranchIsChainedIfElseNotPlainAssignment) {
  const hldb::IfElse *const outer = getOuterIfElse();
  ASSERT_NE(outer, nullptr);
  const hldb::Any *const elseBranch = outer->getElseStmt();
  ASSERT_NE(elseBranch, nullptr) << "outer IfElse must have an else-branch (the chained 'else if')";
  EXPECT_EQ(elseBranch->getAnyType(), hldb::AnyType::IfElse)
      << "'else if(c) d = 1; else b = 0;' has its own trailing else, so it must itself be an IfElse";
  EXPECT_EQ(any_cast<hldb::Assignment>(elseBranch), nullptr)
      << "the chained else-if must not collapse directly into the innermost assignment";
}

TEST_F(IfElseIfTest, InnerIfElseConditionResolvesToNetCNotVariable) {
  const hldb::IfElse *const outer = getOuterIfElse();
  ASSERT_NE(outer, nullptr);
  const hldb::IfElse *const inner = outer->getElseStmt<hldb::IfElse>();
  ASSERT_NE(inner, nullptr);
  const hldb::RefObj *const cond = inner->getCondition<hldb::RefObj>();
  ASSERT_NE(cond, nullptr);
  EXPECT_EQ(cond->getName(), "c");
  EXPECT_NE(cond->getActual<hldb::Net>(), nullptr) << "'c' should resolve to the Net, not a Variable";
}

TEST_F(IfElseIfTest, InnerIfElseThenBranchAssignsOneToVariableD) {
  const hldb::IfElse *const outer = getOuterIfElse();
  ASSERT_NE(outer, nullptr);
  const hldb::IfElse *const inner = outer->getElseStmt<hldb::IfElse>();
  ASSERT_NE(inner, nullptr);
  const hldb::Assignment *const thenAssign = inner->getStmt<hldb::Assignment>();
  ASSERT_NE(thenAssign, nullptr);
  const hldb::RefObj *const lhs = thenAssign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "d");
  EXPECT_NE(lhs->getActual<hldb::Variable>(), nullptr);
  const hldb::Constant *const rhs = thenAssign->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "1");
}

TEST_F(IfElseIfTest, InnerIfElseElseBranchAssignsZeroToVariableB) {
  const hldb::IfElse *const outer = getOuterIfElse();
  ASSERT_NE(outer, nullptr);
  const hldb::IfElse *const inner = outer->getElseStmt<hldb::IfElse>();
  ASSERT_NE(inner, nullptr);
  const hldb::Assignment *const elseAssign = inner->getElseStmt<hldb::Assignment>();
  ASSERT_NE(elseAssign, nullptr) << "the final unconditional else should be a plain Assignment";
  const hldb::RefObj *const lhs = elseAssign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "b");
  EXPECT_NE(lhs->getActual<hldb::Variable>(), nullptr);
  const hldb::Constant *const rhs = elseAssign->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "0");
}

TEST_F(IfElseIfTest, NoContinuousAssigns) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty());
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
