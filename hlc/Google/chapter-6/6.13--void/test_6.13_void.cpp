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

// Tests for 6.13--void.sv (tags: 6.13)
//   module top();
//     function void fun();
//       $display(":assert:(True)");
//     endfunction
//     initial fun();
//   endmodule
//
// What to check and why (IEEE 1800-2023 6.13 "Void data type", p.110,
// checked before any test code was written):
//   "The void data type represents nonexistent data. This type can be
//   specified as the return type of functions to indicate no return
//   value." "function void fun();" is exactly this usage. This file has
//   no :should_fail_because: tag -- it is legal per spec.
//
//   No net or variable is declared anywhere in this file (only a
//   function and an initial block), so the net/variable
//   misclassification bug found elsewhere this session does not apply
//   here.
//
// What is checked:
//   - module top exists, has no nets
//   - exactly 1 Function "fun": return type resolves to VoidTypespec,
//     vpiPublic visibility
//   - exactly 1 Initial process whose body is a FuncCall to "fun" with
//     no arguments
//   - compiler reports zero errors (this file is fully legal per 6.13)
//
// What is NOT checked and why:
//   - none: every corner above is fully structural and checkable without
//     simulation.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/func_call.h>
#include <hldb/function.h>
#include <hldb/initial.h>
#include <hldb/module.h>
#include <hldb/ref_typespec.h>
#include <hldb/tf_call.h>
#include <hldb/void_typespec.h>
#include <hldb/vpi_user.h>

namespace hlc {

class VoidTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.13--void.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }
};

TEST_F(VoidTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(VoidTest, NoNets) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getNets() == nullptr || top->getNets()->empty()) << "module should have no nets";
}

// ---------------------------------------------------------------------------
// Function "fun" -- void return, $display body
// ---------------------------------------------------------------------------
TEST_F(VoidTest, OneFunctionExists) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTaskFuncs(), nullptr) << "module has no task/function declarations";
  EXPECT_EQ(top->getTaskFuncs()->size(), 1u);
}

TEST_F(VoidTest, FunctionNameIsFun) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTaskFuncs(), nullptr);
  const hldb::Function *const fun = any_cast<hldb::Function>(top->getTaskFuncs()->at(0));
  ASSERT_NE(fun, nullptr) << "task/func is not a Function";
  EXPECT_EQ(fun->getName(), "fun");
}

TEST_F(VoidTest, FunctionReturnIsVoidTypespec) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTaskFuncs(), nullptr);
  const hldb::Function *const fun = any_cast<hldb::Function>(top->getTaskFuncs()->at(0));
  ASSERT_NE(fun, nullptr);
  const hldb::RefTypespec *const ret = fun->getReturn();
  ASSERT_NE(ret, nullptr) << "function has no return typespec";
  EXPECT_NE(ret->getActual<hldb::VoidTypespec>(), nullptr) << "function return type should be VoidTypespec";
}

TEST_F(VoidTest, FunctionIsPublic) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTaskFuncs(), nullptr);
  const hldb::Function *const fun = any_cast<hldb::Function>(top->getTaskFuncs()->at(0));
  ASSERT_NE(fun, nullptr);
  EXPECT_EQ(fun->getVisibility(), 1);  // vpiPublic = 1
}

// ---------------------------------------------------------------------------
// Initial process -- initial fun()
// ---------------------------------------------------------------------------
TEST_F(VoidTest, InitialProcessExists) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  EXPECT_EQ(top->getProcesses()->size(), 1u);
}

TEST_F(VoidTest, InitialProcessBodyIsFuncCallToFun) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  const hldb::Initial *const init = dynamic_cast<const hldb::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr) << "process is not an Initial";
  const hldb::FuncCall *const call = init->getStmt<hldb::FuncCall>();
  ASSERT_NE(call, nullptr) << "Initial body is not a FuncCall";
  EXPECT_EQ(call->getName(), "fun");
}

TEST_F(VoidTest, FuncCallHasNoArguments) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = dynamic_cast<const hldb::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::FuncCall *const call = init->getStmt<hldb::FuncCall>();
  ASSERT_NE(call, nullptr);
  EXPECT_TRUE(call->getArguments() == nullptr || call->getArguments()->empty())
      << "fun() is called with no arguments";
}

TEST_F(VoidTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
