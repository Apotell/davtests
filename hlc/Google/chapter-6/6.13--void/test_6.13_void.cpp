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

// Validates the UHDM graph for a module with a void function and an initial
// block that calls it:
//   module top();
//     function void fun();
//       $display(":assert:(True)");
//     endfunction
//     initial fun();
//   endmodule
// Key properties: no nets; Function "fun" with vpiReturn → VoidTypespec;
// Initial process with body FuncCall "fun".

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/func_call.h>
#include <hldb/function.h>
#include <hldb/initial.h>
#include <hldb/module.h>
#include <hldb/ref_typespec.h>
#include <hldb/void_typespec.h>
#include <hldb/vpi_user.h>

namespace hlc {

class Void : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "6.13--void.hlc"});

    ASSERT_NE(m_session, nullptr) << "Session is null";
    ASSERT_NE(m_compiler, nullptr) << "Compiler is null";
    ASSERT_NE(m_design, nullptr) << "Design is null";
  }

  static void TearDownTestSuite() {
    m_design = nullptr;
    delete m_compiler;
    m_compiler = nullptr;
    delete m_session;
    m_session = nullptr;
  }
};

TEST_F(Void, ModuleExists) {
  ASSERT_NE(hldb::findByName<hldb::Module>("work@top", m_design->getAllModules()), nullptr);
}

TEST_F(Void, NoNets) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getNets() == nullptr || top->getNets()->empty())
      << "module should have no nets";
}

// ---------------------------------------------------------------------------
// Function "fun" — void return, $display body
// ---------------------------------------------------------------------------
TEST_F(Void, OneFunctionExists) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTaskFuncs(), nullptr) << "module has no task/function declarations";
  EXPECT_EQ(top->getTaskFuncs()->size(), 1u);
}

TEST_F(Void, FunctionNameIsFun) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTaskFuncs(), nullptr);

  const hldb::Function *const fun =
      any_cast<hldb::Function>(top->getTaskFuncs()->at(0));
  ASSERT_NE(fun, nullptr) << "task/func is not a Function";
  EXPECT_EQ(fun->getName(), "fun");
}

TEST_F(Void, FunctionReturnIsVoidTypespec) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTaskFuncs(), nullptr);

  const hldb::Function *const fun =
      any_cast<hldb::Function>(top->getTaskFuncs()->at(0));
  ASSERT_NE(fun, nullptr);

  const hldb::RefTypespec *const ret = fun->getReturn();
  ASSERT_NE(ret, nullptr) << "function has no return typespec";
  EXPECT_NE(ret->getActual<hldb::VoidTypespec>(), nullptr)
      << "function return type should be VoidTypespec";
}

TEST_F(Void, FunctionIsPublic) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTaskFuncs(), nullptr);

  const hldb::Function *const fun =
      any_cast<hldb::Function>(top->getTaskFuncs()->at(0));
  ASSERT_NE(fun, nullptr);
  EXPECT_EQ(fun->getVisibility(), 1);  // vpiPublic = 1
}

// ---------------------------------------------------------------------------
// Initial process — initial fun()
// ---------------------------------------------------------------------------
TEST_F(Void, InitialProcessExists) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  EXPECT_EQ(top->getProcesses()->size(), 1u);
}

TEST_F(Void, InitialProcessBodyIsFuncCallToFun) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);

  const hldb::Initial *const init =
      dynamic_cast<const hldb::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr) << "process is not an Initial";

  const hldb::FuncCall *const call = init->getStmt<hldb::FuncCall>();
  ASSERT_NE(call, nullptr) << "Initial body is not a FuncCall";
  EXPECT_EQ(call->getName(), "fun");
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
