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

#include <Surelog/Common/Session.h>
#include <Surelog/SourceCompile/Compiler.h>
#include <Surelog/Tests/Test.h>

#include <uhdm/Utils.h>
#include <uhdm/design.h>
#include <uhdm/func_call.h>
#include <uhdm/function.h>
#include <uhdm/initial.h>
#include <uhdm/module.h>
#include <uhdm/ref_typespec.h>
#include <uhdm/void_typespec.h>
#include <uhdm/vpi_user.h>

namespace SURELOG {

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
  ASSERT_NE(uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules()), nullptr);
}

TEST_F(Void, NoNets) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getNets() == nullptr || top->getNets()->empty())
      << "module should have no nets";
}

// ---------------------------------------------------------------------------
// Function "fun" — void return, $display body
// ---------------------------------------------------------------------------
TEST_F(Void, OneFunctionExists) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTaskFuncs(), nullptr) << "module has no task/function declarations";
  EXPECT_EQ(top->getTaskFuncs()->size(), 1u);
}

TEST_F(Void, FunctionNameIsFun) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTaskFuncs(), nullptr);

  const uhdm::Function *const fun =
      any_cast<uhdm::Function>(top->getTaskFuncs()->at(0));
  ASSERT_NE(fun, nullptr) << "task/func is not a Function";
  EXPECT_EQ(fun->getName(), "fun");
}

TEST_F(Void, FunctionReturnIsVoidTypespec) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTaskFuncs(), nullptr);

  const uhdm::Function *const fun =
      any_cast<uhdm::Function>(top->getTaskFuncs()->at(0));
  ASSERT_NE(fun, nullptr);

  const uhdm::RefTypespec *const ret = fun->getReturn();
  ASSERT_NE(ret, nullptr) << "function has no return typespec";
  EXPECT_NE(ret->getActual<uhdm::VoidTypespec>(), nullptr)
      << "function return type should be VoidTypespec";
}

TEST_F(Void, FunctionIsPublic) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTaskFuncs(), nullptr);

  const uhdm::Function *const fun =
      any_cast<uhdm::Function>(top->getTaskFuncs()->at(0));
  ASSERT_NE(fun, nullptr);
  EXPECT_EQ(fun->getVisibility(), 1);  // vpiPublic = 1
}

// ---------------------------------------------------------------------------
// Initial process — initial fun()
// ---------------------------------------------------------------------------
TEST_F(Void, InitialProcessExists) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  EXPECT_EQ(top->getProcesses()->size(), 1u);
}

TEST_F(Void, InitialProcessBodyIsFuncCallToFun) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);

  const uhdm::Initial *const init =
      dynamic_cast<const uhdm::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr) << "process is not an Initial";

  const uhdm::FuncCall *const call = init->getStmt<uhdm::FuncCall>();
  ASSERT_NE(call, nullptr) << "Initial body is not a FuncCall";
  EXPECT_EQ(call->getName(), "fun");
}

}  // namespace SURELOG

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
