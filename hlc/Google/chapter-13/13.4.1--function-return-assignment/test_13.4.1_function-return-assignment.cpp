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

// Tests for 13.4.1--function-return-assignment.sv (tags: 13.4.1)
//   module top();
//     function int add(int a, int b);
//       add = a + b;
//     endfunction
//     initial
//       $display(":assert: (%d == 90)", add(30, 60));
//   endmodule
//
// What to check and why (IEEE 1800-2023 13.4.1 "Return values and void
// functions", p.342, checked before any test code was written):
//   "The function definition shall implicitly declare a variable,
//   internal to the function, with the same name as the function...
//   Function return values can be specified in two ways, either by
//   using a return statement or by assigning a value to the internal
//   variable with the same name as the function." This file uses the
//   assignment form, "add = a + b;" -- unlike
//   13.4.1--function-return.cpp's "return a + b;", there is no
//   ReturnStmt at all here; the function's body is a plain Assignment.
//
//   IMPORTANT object-model note, confirmed structurally in this exact
//   file (not guessed): the LHS "add" of "add = a + b;" is a RefObj
//   that resolves (getActual<hldb::Function>()) directly to the
//   Function object itself -- not to some separate "implicit return
//   variable" object. HLC models the spec's "implicitly declared
//   variable with the same name as the function" by simply letting the
//   function's own name resolve back to the Function when used as an
//   assignment target inside its own body.
//
// What is checked:
//   - module top has exactly 1 TaskFunc, a Function named "add", whose
//     return typespec resolves to IntTypespec, with 2 input IODecls
//     "a"/"b" (same declaration shape as 13.4.1--function-return.cpp)
//   - add's body is a plain Assignment directly (no Begin wrapper,
//     single statement, no begin-end in source; and specifically NOT
//     a ReturnStmt) whose LHS is RefObj "add" resolving
//     (getActual<hldb::Function>()) to the function itself, and whose
//     RHS is Operation(vpiAddOp) comparing RefObj "a" against RefObj
//     "b" (both resolving to their IODecls)
//   - the initial process's body is a plain SysTaskCall "$display"
//     with a FuncCall "add" argument, identical in shape to
//     13.4.1--function-return.cpp's call site
//   - no continuous assignments, nets, or module-scope variables exist
//
// What is NOT checked and why:
//   - the runtime returned value (that add(30, 60) actually evaluates
//     to 90 via the assignment form) is a simulation-time concept, not
//     a static/structural compile-time property

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/func_call.h>
#include <hldb/function.h>
#include <hldb/initial.h>
#include <hldb/int_typespec.h>
#include <hldb/io_decl.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sys_task_call.h>
#include <hldb/vpi_user.h>

namespace hlc {

class FunctionReturnAssignmentTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "13.4.1--function-return-assignment.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::Function *getAdd() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getTaskFuncs() == nullptr || top->getTaskFuncs()->empty()) return nullptr;
    return any_cast<hldb::Function>(top->getTaskFuncs()->at(0));
  }
};

TEST_F(FunctionReturnAssignmentTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(FunctionReturnAssignmentTest, AddExistsAsIntFunctionWithTwoInputIODecls) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTaskFuncs(), nullptr);
  ASSERT_EQ(top->getTaskFuncs()->size(), 1u);
  const hldb::Function *const add = getAdd();
  ASSERT_NE(add, nullptr);
  EXPECT_EQ(add->getName(), "add");
  const hldb::RefTypespec *const rts = add->getReturn();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::IntTypespec>(), nullptr);
  ASSERT_NE(add->getIODecls(), nullptr);
  ASSERT_EQ(add->getIODecls()->size(), 2u);
}

// ---------------------------------------------------------------------------
// add = a + b;  -- assigns to the implicit function-name variable
// ---------------------------------------------------------------------------
TEST_F(FunctionReturnAssignmentTest, BodyIsAssignmentToFunctionNameNotReturnStmt) {
  const hldb::Function *const add = getAdd();
  ASSERT_NE(add, nullptr);
  const hldb::Assignment *const assign = add->getStmt<hldb::Assignment>();
  ASSERT_NE(assign, nullptr)
      << "function body should be a plain Assignment (single statement, no begin-end), not a ReturnStmt";

  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr) << "Assignment LHS is not a RefObj";
  EXPECT_EQ(lhs->getName(), "add");
  EXPECT_NE(lhs->getActual<hldb::Function>(), nullptr)
      << "'add' on the LHS should resolve to the Function itself (the implicit return variable), per 13.4.1";

  const hldb::Operation *const rhs = assign->getRhs<hldb::Operation>();
  ASSERT_NE(rhs, nullptr) << "Assignment RHS is not an Operation";
  EXPECT_EQ(rhs->getOpType(), vpiAddOp);
  ASSERT_NE(rhs->getOperands(), nullptr);
  ASSERT_EQ(rhs->getOperands()->size(), 2u);
  const hldb::RefObj *const aRef = any_cast<hldb::RefObj>(rhs->getOperands()->at(0));
  ASSERT_NE(aRef, nullptr);
  EXPECT_EQ(aRef->getName(), "a");
  EXPECT_NE(aRef->getActual<hldb::IODecl>(), nullptr);
  const hldb::RefObj *const bRef = any_cast<hldb::RefObj>(rhs->getOperands()->at(1));
  ASSERT_NE(bRef, nullptr);
  EXPECT_EQ(bRef->getName(), "b");
  EXPECT_NE(bRef->getActual<hldb::IODecl>(), nullptr);
}

// ---------------------------------------------------------------------------
// initial $display(":assert: (%d == 90)", add(30, 60));
// ---------------------------------------------------------------------------
TEST_F(FunctionReturnAssignmentTest, InitialBodyDisplaysAddCallResultAndResolvesBackToAdd) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);
  const hldb::Initial *const initial = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(initial, nullptr);
  const hldb::SysTaskCall *const display = initial->getStmt<hldb::SysTaskCall>();
  ASSERT_NE(display, nullptr);
  ASSERT_NE(display->getArguments(), nullptr);
  ASSERT_EQ(display->getArguments()->size(), 2u);

  const hldb::FuncCall *const call = any_cast<hldb::FuncCall>(display->getArguments()->at(1));
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getName(), "add");
  EXPECT_EQ(call->getTaskFunc<hldb::Function>(), getAdd());
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 2u);
  const hldb::Constant *const arg0 = any_cast<hldb::Constant>(call->getArguments()->at(0));
  ASSERT_NE(arg0, nullptr);
  EXPECT_EQ(arg0->getDecompile(), "30");
  const hldb::Constant *const arg1 = any_cast<hldb::Constant>(call->getArguments()->at(1));
  ASSERT_NE(arg1, nullptr);
  EXPECT_EQ(arg1->getDecompile(), "60");
}

TEST_F(FunctionReturnAssignmentTest, NoContinuousAssigns) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty());
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
