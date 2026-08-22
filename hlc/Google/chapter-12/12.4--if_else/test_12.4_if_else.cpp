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

// Tests for 12.4--if_else.sv (tags: 12.4)
//   module if_tb ();
//     wire a = 0;
//     reg b = 0;
//     always @* begin
//       if(a) b = 1;
//       else b = 0;
//     end
//   endmodule
//
// What to check and why (IEEE 1800-2023 12.4 "Conditional if-else
// statement", p.315, checked before any test code was written):
//   "conditional_statement ::= [ unique_priority ] if ( cond_predicate )
//   statement_or_null { else if (...) statement_or_null } [ else
//   statement_or_null ]" -- this file adds a trailing "else
//   statement_or_null" (no "else if" chain), so this is the simplest
//   if-else form. "If there is an else statement and the cond_predicate
//   expression is false, the else statement shall be executed." Unlike
//   12.4--if.sv (no else), this must build an if-else object (a
//   different UHDM type from the plain if-statement), with a populated
//   else branch.
//
//   Also (IEEE 1800-2023 6.7/6.8): "wire" is a net_type keyword so
//   "wire a" must be a Net; "reg" is a non-net integer_vector_type
//   keyword so "reg b" must be a Variable, never a Net.
//
// What is checked:
//   - module if_tb has exactly 1 Net "a" (wire) and 1 Variable "b" (reg)
//   - the always body's single statement is specifically IfElse
//     (AnyType::IfElse), not a plain IfStmt
//   - IfElse qualifier is vpiNoQualifier (no unique/unique0/priority
//     keyword present)
//   - IfElse condition is RefObj "a" resolving to the Net "a"
//   - IfElse "then" body is Assignment "b = 1" (LHS resolves to
//     Variable "b", RHS Constant "1")
//   - IfElse "else" body is Assignment "b = 0" (LHS resolves to
//     Variable "b", RHS Constant "0") -- present directly (not wrapped
//     in a Begin) because it is a single statement
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

class IfElseTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "12.4--if_else.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("if_tb", m_design->getAllModules()); }

  static const hldb::IfElse *getIfElse() {
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

TEST_F(IfElseTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

// ---------------------------------------------------------------------------
// Declarations -- wire 'a' is a Net, reg 'b' is a Variable
// ---------------------------------------------------------------------------
TEST_F(IfElseTest, ModuleHasOneNetAAndOneVariableB) {
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
// The lone statement is an IfElse (has a trailing else)
// ---------------------------------------------------------------------------
TEST_F(IfElseTest, LoneStmtIsIfElse) {
  const hldb::IfElse *const ifElse = getIfElse();
  ASSERT_NE(ifElse, nullptr) << "the single statement in the always body should resolve to IfElse";
  EXPECT_EQ(ifElse->getAnyType(), hldb::AnyType::IfElse);
}

TEST_F(IfElseTest, IfElseQualifierIsNoQualifier) {
  const hldb::IfElse *const ifElse = getIfElse();
  ASSERT_NE(ifElse, nullptr);
  EXPECT_EQ(ifElse->getQualifier(), vpiNoQualifier) << "no unique/unique0/priority keyword precedes this if";
}

TEST_F(IfElseTest, ConditionResolvesToNetANotVariable) {
  const hldb::IfElse *const ifElse = getIfElse();
  ASSERT_NE(ifElse, nullptr);
  const hldb::RefObj *const cond = ifElse->getCondition<hldb::RefObj>();
  ASSERT_NE(cond, nullptr) << "IfElse condition is not a RefObj";
  EXPECT_EQ(cond->getName(), "a");
  EXPECT_NE(cond->getActual<hldb::Net>(), nullptr) << "'a' should resolve to the Net, not a Variable";
}

TEST_F(IfElseTest, ThenBranchAssignsOneToVariableB) {
  const hldb::IfElse *const ifElse = getIfElse();
  ASSERT_NE(ifElse, nullptr);
  const hldb::Assignment *const thenAssign = ifElse->getStmt<hldb::Assignment>();
  ASSERT_NE(thenAssign, nullptr) << "IfElse 'then' body is not an Assignment";
  const hldb::RefObj *const lhs = thenAssign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "b");
  EXPECT_NE(lhs->getActual<hldb::Variable>(), nullptr) << "'b' should resolve to the Variable, not a Net";
  const hldb::Constant *const rhs = thenAssign->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "1");
}

TEST_F(IfElseTest, ElseBranchAssignsZeroToVariableB) {
  const hldb::IfElse *const ifElse = getIfElse();
  ASSERT_NE(ifElse, nullptr);
  const hldb::Assignment *const elseAssign = ifElse->getElseStmt<hldb::Assignment>();
  ASSERT_NE(elseAssign, nullptr) << "IfElse 'else' body is not an Assignment (should not be wrapped in a Begin)";
  const hldb::RefObj *const lhs = elseAssign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "b");
  EXPECT_NE(lhs->getActual<hldb::Variable>(), nullptr) << "'b' should resolve to the Variable, not a Net";
  const hldb::Constant *const rhs = elseAssign->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "0");
}

TEST_F(IfElseTest, NoContinuousAssigns) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty());
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
