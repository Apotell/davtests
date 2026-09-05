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

// Tests for 13.4.2--function-static.sv (tags: 13.4.2)
//   module top();
//     function static int add(int val);
//       int a = 0;
//       a = a + val;
//       return a;
//     endfunction
//     initial
//       begin
//         $display(":assert: (%d == 5)", add(5));
//         $display(":assert: (%d == 10)", add(5));
//         $display(":assert: (%d == 15)", add(5));
//         $display(":assert: (%d == 20)", add(5));
//       end
//   endmodule
//
// What to check and why (IEEE 1800-2023 13.4.2 "Static and automatic
// functions", p.344, checked before any test code was written):
//   "Functions defined within a module... default to being static,
//   with all declared items being statically allocated. These items
//   shall be shared across all uses of the function executing
//   concurrently." The explicit "static" keyword here reaffirms the
//   default lifetime, so getAutomatic() should be false -- contrast
//   with 13.4.2--function-automatic.cpp's explicit "automatic". The
//   expected messages in this file's own source are the tell: they
//   accumulate (5, 10, 15, 20) rather than repeating a constant 5,
//   which is exactly what "shared, statically allocated storage"
//   implies for "a" across calls.
//
//   Also (IEEE 1800-2023 6.8): "int" is a non-net data type, so "a"
//   must be a Variable, and the IODecl "val" carries an IntTypespec.
//
// What is checked:
//   - module top has exactly 1 TaskFunc, a Function named "add", whose
//     return typespec resolves to IntTypespec, with exactly 1 IODecl
//     "val" (getDirection() == vpiInput, IntTypespec)
//   - add's lifetime is NOT automatic (getAutomatic() == false)
//   - add's body is a Begin whose own scope has exactly 1 Variable "a"
//     (initial value Constant "0"), and whose getStmts() has exactly 3
//     entries: the Variable "a" declaration itself, an Assignment
//     ("a = a + val;"), and a ReturnStmt whose getCondition() is
//     RefObj "a" (same shape as 13.4.2--function-automatic.cpp's body)
//   - the initial process's body is a Begin with exactly 4 SysTaskCall
//     "$display" statements, each with a FuncCall "add" argument
//     (Constant "5" every time) resolving back to the declaration --
//     but with 4 DIFFERENT format-string Constants ("\":assert: (%d ==
//     5)\"", "10", "15", "20"), documenting the static-storage
//     accumulation the source itself expects
//   - no continuous assignments or module-scope variables exist
//
// What is NOT checked and why:
//   - the runtime "static storage persists and accumulates across
//     calls" behavior itself (that add(5) really does return 5, 10,
//     15, then 20 in sequence) is a simulation-time concept, not a
//     static/structural compile-time property

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
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
#include <hldb/return_stmt.h>
#include <hldb/sys_task_call.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class FunctionStaticTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "13.4.2--function-static.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::Function *getAdd() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getTaskFuncs() == nullptr || top->getTaskFuncs()->empty()) return nullptr;
    return any_cast<hldb::Function>(top->getTaskFuncs()->at(0));
  }
};

TEST_F(FunctionStaticTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(FunctionStaticTest, AddExistsAsIntFunctionWithOneInputIODeclVal) {
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
  ASSERT_EQ(add->getIODecls()->size(), 1u);
  const hldb::IODecl *const val = add->getIODecls()->at(0);
  ASSERT_NE(val, nullptr);
  EXPECT_EQ(val->getName(), "val");
  EXPECT_EQ(val->getDirection(), vpiInput);
}

TEST_F(FunctionStaticTest, AddLifetimeIsNotAutomatic) {
  const hldb::Function *const add = getAdd();
  ASSERT_NE(add, nullptr);
  EXPECT_FALSE(add->getAutomatic())
      << "'function static int add(...)' explicitly reaffirms the default static lifetime, per 13.4.2";
}

// ---------------------------------------------------------------------------
// int a = 0; a = a + val; return a;
// ---------------------------------------------------------------------------
TEST_F(FunctionStaticTest, BodyScopeHasVariableAInitializedToZero) {
  const hldb::Function *const add = getAdd();
  ASSERT_NE(add, nullptr);
  const hldb::Begin *const body = add->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr) << "function body should be a Begin";
  ASSERT_NE(body->getVariables(), nullptr);
  ASSERT_EQ(body->getVariables()->size(), 1u);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", body->getVariables());
  ASSERT_NE(a, nullptr);
  const hldb::Constant *const init = a->getValue<hldb::Constant>();
  ASSERT_NE(init, nullptr);
  EXPECT_EQ(init->getDecompile(), "0");
}

TEST_F(FunctionStaticTest, BodyHasThreeStatementsDeclAssignReturn) {
  const hldb::Function *const add = getAdd();
  ASSERT_NE(add, nullptr);
  const hldb::Begin *const body = add->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr);
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 3u)
      << "function body statements include the 'int a = 0;' declaration itself as entry 0";

  const hldb::Variable *const declEntry = any_cast<hldb::Variable>(body->getStmts()->at(0));
  ASSERT_NE(declEntry, nullptr);
  EXPECT_EQ(declEntry->getName(), "a");

  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(body->getStmts()->at(1));
  ASSERT_NE(assign, nullptr) << "'a = a + val;' should be an Assignment";
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "a");
  const hldb::Operation *const rhs = assign->getRhs<hldb::Operation>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getOpType(), vpiAddOp);

  const hldb::ReturnStmt *const ret = any_cast<hldb::ReturnStmt>(body->getStmts()->at(2));
  ASSERT_NE(ret, nullptr);
  const hldb::RefObj *const retExpr = ret->getCondition<hldb::RefObj>();
  ASSERT_NE(retExpr, nullptr);
  EXPECT_EQ(retExpr->getName(), "a");
  EXPECT_NE(retExpr->getActual<hldb::Variable>(), nullptr);
}

// ---------------------------------------------------------------------------
// initial begin $display(":assert: (%d == N)", add(5)); N=5,10,15,20
// ---------------------------------------------------------------------------
TEST_F(FunctionStaticTest, InitialBodyCallsAddFourTimesWithAccumulatingExpectedMessages) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);
  const hldb::Initial *const initial = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(initial, nullptr);
  const hldb::Begin *const body = initial->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr) << "initial body should be a Begin (explicit begin-end in source)";
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 4u);

  const std::string_view expectedMessages[4] = {"\":assert: (%d == 5)\"", "\":assert: (%d == 10)\"",
                                                 "\":assert: (%d == 15)\"", "\":assert: (%d == 20)\""};
  for (uint32_t idx = 0; idx < 4u; ++idx) {
    const hldb::SysTaskCall *const display = any_cast<hldb::SysTaskCall>(body->getStmts()->at(idx));
    ASSERT_NE(display, nullptr);
    ASSERT_NE(display->getArguments(), nullptr);
    ASSERT_EQ(display->getArguments()->size(), 2u);

    const hldb::Constant *const fmt = any_cast<hldb::Constant>(display->getArguments()->at(0));
    ASSERT_NE(fmt, nullptr);
    EXPECT_EQ(fmt->getDecompile(), expectedMessages[idx])
        << "the accumulating expected message documents this function's static (shared) storage semantics";

    const hldb::FuncCall *const call = any_cast<hldb::FuncCall>(display->getArguments()->at(1));
    ASSERT_NE(call, nullptr);
    EXPECT_EQ(call->getName(), "add");
    EXPECT_EQ(call->getTaskFunc<hldb::Function>(), getAdd());
    ASSERT_NE(call->getArguments(), nullptr);
    ASSERT_EQ(call->getArguments()->size(), 1u);
    const hldb::Constant *const arg = any_cast<hldb::Constant>(call->getArguments()->at(0));
    ASSERT_NE(arg, nullptr);
    EXPECT_EQ(arg->getDecompile(), "5") << "every call passes the same argument, 5";
  }
}

TEST_F(FunctionStaticTest, NoContinuousAssigns) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty());
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
