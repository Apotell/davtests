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

// Tests for 12.7.4--while.sv (tags: 12.7.4)
//   module while_tb ();
//     string test [4] = '{"111", "222", "333", "444"};
//     initial begin
//       int i = 0;
//       while(test[i] != "222")begin
//         $display(i, test[i]);
//         i++;
//       end
//     end
//   endmodule
//
// What to check and why (IEEE 1800-2023 12.7.4 "The while-loop", p.332,
// checked before any test code was written):
//   "The while-loop repeatedly executes a statement as long as a
//   control expression is true (as defined in 12.4). If the expression
//   is not true at the beginning of the execution of the while-loop,
//   the statement shall not be executed at all." Unlike foreach/for,
//   "int i = 0" here is declared as an ordinary statement inside the
//   enclosing "initial begin", not inside the while-loop's own syntax
//   -- so "i" belongs to the outer Begin's scope, and WhileStmt itself
//   has no scope/local variables of its own (it is an AtomicStmt, not a
//   Scope).
//
//   Also (IEEE 1800-2023 6.8): "string"/"int" are data types, not
//   net_type keywords, so "test" and "i" must be Variables, never Nets.
//
// What is checked:
//   - module while_tb has zero Nets and exactly 1 Variable "test"
//   - the enclosing "initial begin" Begin has exactly 1 Variable "i"
//     with initial value Constant "0" -- confirming "i" is NOT scoped
//     to the while-loop itself (contrast with 12.7.1--for.cpp and
//     12.7.3--foreach.cpp, where the loop variable is scoped to the
//     loop)
//   - WhileStmt condition is Operation(vpiNeqOp) comparing BitSelect
//     "test[i]" (prefix resolves to Variable "test", index resolves to
//     Variable "i") against Constant "\"222\""
//   - WhileStmt body is a Begin with exactly 2 statements: SysTaskCall
//     "$display" (arguments RefObj "i" and BitSelect "test[i]") and a
//     standalone Operation(vpiPostIncOp) on RefObj "i" (the "i++;"
//     statement, appearing directly as a statement, not wrapped in an
//     Assignment)
//   - no continuous assignments exist in the module
//
// What is NOT checked and why:
//   - the exact contents of the array's '{"111","222","333","444"}
//     initializer are chapter-7/11 territory; only existence of "test"
//     is checked here
//   - the runtime iteration behavior itself (how many times the loop
//     body actually executes) is a simulation-time concept, not a
//     static/structural compile-time property

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/begin.h>
#include <hldb/bit_select.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/initial.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/sys_task_call.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>
#include <hldb/while_stmt.h>

namespace hlc {

class WhileTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "12.7.4--while.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() {
    return hldb::findByName<hldb::Module>("while_tb", m_design->getAllModules());
  }

  static const hldb::Begin *getInitialBody() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getProcesses() == nullptr || top->getProcesses()->empty()) return nullptr;
    const hldb::Initial *const initial = any_cast<hldb::Initial>(top->getProcesses()->at(0));
    if (initial == nullptr) return nullptr;
    return initial->getStmt<hldb::Begin>();
  }

  static const hldb::WhileStmt *getWhileStmt() {
    const hldb::Begin *const body = getInitialBody();
    if (body == nullptr || body->getStmts() == nullptr || body->getStmts()->empty()) return nullptr;
    return any_cast<hldb::WhileStmt>(body->getStmts()->at(0));
  }
};

TEST_F(WhileTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(WhileTest, ModuleHasNoNetsAndOneVariableTest) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getNets() == nullptr || top->getNets()->empty()) << "no wire/net_type keyword is declared";
  ASSERT_NE(top->getVariables(), nullptr) << "'string test[4]' should be a Variable, not a Net";
  EXPECT_NE(hldb::findByName<hldb::Variable>("test", top->getVariables()), nullptr) << "Variable 'test' not found";
}

// ---------------------------------------------------------------------------
// "int i = 0" is declared in the outer Begin, NOT scoped to the while
// ---------------------------------------------------------------------------
TEST_F(WhileTest, InitialBeginScopeHasVariableIInitializedToZero) {
  const hldb::Begin *const body = getInitialBody();
  ASSERT_NE(body, nullptr);
  ASSERT_NE(body->getVariables(), nullptr) << "'int i' should live in the enclosing Begin's scope";
  const hldb::Variable *const i = hldb::findByName<hldb::Variable>("i", body->getVariables());
  ASSERT_NE(i, nullptr) << "Variable 'i' not found";
  const hldb::Constant *const init = i->getValue<hldb::Constant>();
  ASSERT_NE(init, nullptr);
  EXPECT_EQ(init->getDecompile(), "0");
}

TEST_F(WhileTest, OnlyStmtInBeginIsWhileStmt) {
  const hldb::WhileStmt *const w = getWhileStmt();
  ASSERT_NE(w, nullptr)
      << "'int i = 0' is a declaration (lives in getVariables()), not a statement, so the while-loop should be "
         "the Begin's only entry in getStmts()";
}

// ---------------------------------------------------------------------------
// while(test[i] != "222")
// ---------------------------------------------------------------------------
TEST_F(WhileTest, ConditionIsTestOfINotEqualTo222) {
  const hldb::WhileStmt *const w = getWhileStmt();
  ASSERT_NE(w, nullptr);
  const hldb::Operation *const cond = w->getCondition<hldb::Operation>();
  ASSERT_NE(cond, nullptr) << "while condition is not an Operation";
  EXPECT_EQ(cond->getOpType(), vpiNeqOp);
  ASSERT_NE(cond->getOperands(), nullptr);
  ASSERT_EQ(cond->getOperands()->size(), 2u);

  const hldb::BitSelect *const testI = any_cast<hldb::BitSelect>(cond->getOperands()->at(0));
  ASSERT_NE(testI, nullptr) << "first operand should be BitSelect 'test[i]'";
  EXPECT_EQ(testI->getName(), "test[i]");
  const hldb::RefObj *const prefix = testI->getPrefix<hldb::RefObj>();
  ASSERT_NE(prefix, nullptr);
  EXPECT_NE(prefix->getActual<hldb::Variable>(), nullptr) << "'test' should resolve to the array Variable";
  const hldb::RefObj *const index = testI->getIndex<hldb::RefObj>();
  ASSERT_NE(index, nullptr);
  EXPECT_NE(index->getActual<hldb::Variable>(), nullptr) << "'i' index should resolve to the Variable";

  const hldb::Constant *const rhs = any_cast<hldb::Constant>(cond->getOperands()->at(1));
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getConstType(), vpiStringConst);
  EXPECT_EQ(rhs->getDecompile(), "\"222\"");
}

// ---------------------------------------------------------------------------
// begin $display(i, test[i]); i++; end
// ---------------------------------------------------------------------------
TEST_F(WhileTest, BodyIsBeginWithDisplayAndPostIncrement) {
  const hldb::WhileStmt *const w = getWhileStmt();
  ASSERT_NE(w, nullptr);
  const hldb::Begin *const body = w->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr) << "while body should be a Begin (explicit begin-end in source)";
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 2u);

  const hldb::SysTaskCall *const display = any_cast<hldb::SysTaskCall>(body->getStmts()->at(0));
  ASSERT_NE(display, nullptr);
  EXPECT_EQ(display->getName(), "$display");
  ASSERT_NE(display->getArguments(), nullptr);
  ASSERT_EQ(display->getArguments()->size(), 2u);

  const hldb::Operation *const inc = any_cast<hldb::Operation>(body->getStmts()->at(1));
  ASSERT_NE(inc, nullptr) << "'i++;' should be a standalone post-increment Operation statement";
  EXPECT_EQ(inc->getOpType(), vpiPostIncOp);
  ASSERT_NE(inc->getOperands(), nullptr);
  ASSERT_EQ(inc->getOperands()->size(), 1u);
  const hldb::RefObj *const operand = any_cast<hldb::RefObj>(inc->getOperands()->at(0));
  ASSERT_NE(operand, nullptr);
  EXPECT_EQ(operand->getName(), "i");
}

TEST_F(WhileTest, NoContinuousAssigns) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty());
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
