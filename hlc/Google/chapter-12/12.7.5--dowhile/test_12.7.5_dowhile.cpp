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

// Tests for 12.7.5--dowhile.sv (tags: 12.7.5)
//   module dowhile_tb ();
//     string test [4] = '{"111", "222", "333", "444"};
//     initial begin
//       int i = 0;
//       do begin
//         $display(i, test[i]);
//         i++;
//       end while(test[i] != "222");
//     end
//   endmodule
//
// What to check and why (IEEE 1800-2023 12.7.5 "The do...while-loop",
// p.332, checked before any test code was written):
//   "The do...while-loop differs from the while-loop in that a
//   do...while-loop tests its control expression at the end of the
//   loop... The condition can be any expression that can be treated as
//   a Boolean. It is evaluated after the statement." Structurally this
//   is the same shape as 12.7.4--while.sv (a condition Operation and a
//   body statement), just evaluated at a different point in execution
//   -- a DoWhile object rather than a WhileStmt object, but with the
//   same condition/body field layout. As in the while-loop file, "int
//   i = 0" is declared as an ordinary statement inside the enclosing
//   "initial begin", not inside the loop's own syntax.
//
//   Also (IEEE 1800-2023 6.8): "string"/"int" are data types, not
//   net_type keywords, so "test" and "i" must be Variables, never Nets.
//
// What is checked:
//   - module dowhile_tb has zero Nets and exactly 1 Variable "test"
//   - the enclosing "initial begin" Begin has exactly 1 Variable "i"
//     with initial value Constant "0"
//   - the second statement in that Begin resolves to DoWhile
//     (AnyType::DoWhile), confirming this is modeled as a distinct
//     object from WhileStmt even though the condition/body shape is
//     identical
//   - DoWhile condition is Operation(vpiNeqOp) comparing BitSelect
//     "test[i]" (prefix resolves to Variable "test", index resolves to
//     Variable "i") against Constant "\"222\""
//   - DoWhile body is a Begin with exactly 2 statements: SysTaskCall
//     "$display" (arguments RefObj "i" and BitSelect "test[i]") and a
//     standalone Operation(vpiPostIncOp) on RefObj "i"
//   - no continuous assignments exist in the module
//
// What is NOT checked and why:
//   - the exact contents of the array's '{"111","222","333","444"}
//     initializer are chapter-7/11 territory; only existence of "test"
//     is checked here
//   - the runtime "condition tested after the body, so the body always
//     executes at least once" behavior is a simulation-time concept,
//     not a static/structural compile-time property

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/begin.h>
#include <hldb/bit_select.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/do_while.h>
#include <hldb/initial.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/sys_task_call.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class DoWhileTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "12.7.5--dowhile.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() {
    return hldb::findByName<hldb::Module>("dowhile_tb", m_design->getAllModules());
  }

  static const hldb::Begin *getInitialBody() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getProcesses() == nullptr || top->getProcesses()->empty()) return nullptr;
    const hldb::Initial *const initial = any_cast<hldb::Initial>(top->getProcesses()->at(0));
    if (initial == nullptr) return nullptr;
    return initial->getStmt<hldb::Begin>();
  }

  static const hldb::DoWhile *getDoWhile() {
    const hldb::Begin *const body = getInitialBody();
    if (body == nullptr || body->getStmts() == nullptr || body->getStmts()->empty()) return nullptr;
    return any_cast<hldb::DoWhile>(body->getStmts()->at(0));
  }
};

TEST_F(DoWhileTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(DoWhileTest, ModuleHasNoNetsAndOneVariableTest) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getNets() == nullptr || top->getNets()->empty()) << "no wire/net_type keyword is declared";
  ASSERT_NE(top->getVariables(), nullptr) << "'string test[4]' should be a Variable, not a Net";
  EXPECT_NE(hldb::findByName<hldb::Variable>("test", top->getVariables()), nullptr) << "Variable 'test' not found";
}

TEST_F(DoWhileTest, InitialBeginScopeHasVariableIInitializedToZero) {
  const hldb::Begin *const body = getInitialBody();
  ASSERT_NE(body, nullptr);
  ASSERT_NE(body->getVariables(), nullptr) << "'int i' should live in the enclosing Begin's scope";
  const hldb::Variable *const i = hldb::findByName<hldb::Variable>("i", body->getVariables());
  ASSERT_NE(i, nullptr) << "Variable 'i' not found";
  const hldb::Constant *const init = i->getValue<hldb::Constant>();
  ASSERT_NE(init, nullptr);
  EXPECT_EQ(init->getDecompile(), "0");
}

TEST_F(DoWhileTest, OnlyStmtInBeginIsDoWhile) {
  const hldb::DoWhile *const dw = getDoWhile();
  ASSERT_NE(dw, nullptr)
      << "'int i = 0' is a declaration (lives in getVariables()), not a statement, so the do-while should be "
         "the Begin's only entry in getStmts()";
  EXPECT_EQ(dw->getAnyType(), hldb::AnyType::DoWhile);
}

// ---------------------------------------------------------------------------
// while(test[i] != "222")  -- tested at the end, but same field layout
// ---------------------------------------------------------------------------
TEST_F(DoWhileTest, ConditionIsTestOfINotEqualTo222) {
  const hldb::DoWhile *const dw = getDoWhile();
  ASSERT_NE(dw, nullptr);
  const hldb::Operation *const cond = dw->getCondition<hldb::Operation>();
  ASSERT_NE(cond, nullptr) << "do-while condition is not an Operation";
  EXPECT_EQ(cond->getOpType(), vpiNeqOp);
  ASSERT_NE(cond->getOperands(), nullptr);
  ASSERT_EQ(cond->getOperands()->size(), 2u);

  const hldb::BitSelect *const testI = any_cast<hldb::BitSelect>(cond->getOperands()->at(0));
  ASSERT_NE(testI, nullptr) << "first operand should be BitSelect 'test[i]'";
  EXPECT_EQ(testI->getName(), "test[i]");
  const hldb::RefObj *const prefix = testI->getPrefix<hldb::RefObj>();
  ASSERT_NE(prefix, nullptr);
  EXPECT_NE(prefix->getActual<hldb::Variable>(), nullptr) << "'test' should resolve to the array Variable";

  const hldb::Constant *const rhs = any_cast<hldb::Constant>(cond->getOperands()->at(1));
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getConstType(), vpiStringConst);
  EXPECT_EQ(rhs->getDecompile(), "\"222\"");
}

// ---------------------------------------------------------------------------
// do begin $display(i, test[i]); i++; end while(...)
// ---------------------------------------------------------------------------
TEST_F(DoWhileTest, BodyIsBeginWithDisplayAndPostIncrement) {
  const hldb::DoWhile *const dw = getDoWhile();
  ASSERT_NE(dw, nullptr);
  const hldb::Begin *const body = dw->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr) << "do-while body should be a Begin (explicit begin-end in source)";
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

TEST_F(DoWhileTest, NoContinuousAssigns) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty());
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
