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

// Validates that a bare system function call compiles and appears in UHDM:
//   initial $display("hello world");
//
// UHDM structure:
//   Module name:work@systemfn
//     Initial
//       vpiStmt: SysFuncCall "$display"   ← direct stmt, NO Begin wrapper
//         vpiArgument[0]: Constant (vpiStringConst=6), getValue()="hello world"
//
// Notable difference from the builtin-methods tests: the SysFuncCall is the
// direct statement of the Initial block, not wrapped in a Begin.

#include <Surelog/Common/Session.h>
#include <Surelog/SourceCompile/Compiler.h>
#include <Surelog/Tests/Test.h>

#include <uhdm/Utils.h>
#include <uhdm/constant.h>
#include <uhdm/design.h>
#include <uhdm/initial.h>
#include <uhdm/module.h>
#include <uhdm/sys_func_call.h>

namespace SURELOG {

class SystemFunctions : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "5.6.3--system-functions.hlc"});

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

static const uhdm::SysFuncCall *getDisplay(const uhdm::Design *d) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@systemfn", d->getAllModules());
  if (!top || !top->getProcesses()) return nullptr;
  for (const uhdm::Process *const p : *top->getProcesses()) {
    if (const uhdm::Initial *const i = any_cast<uhdm::Initial>(p))
      return i->getStmt<uhdm::SysFuncCall>();
  }
  return nullptr;
}

// ---------------------------------------------------------------------------
// Module
// ---------------------------------------------------------------------------
TEST_F(SystemFunctions, ModuleExists) {
  ASSERT_NE(
      uhdm::findByName<uhdm::Module>("work@systemfn", m_design->getAllModules()),
      nullptr)
      << "module 'work@systemfn' not found";
}

// ---------------------------------------------------------------------------
// initial $display("hello world")
// ---------------------------------------------------------------------------
TEST_F(SystemFunctions, InitialHasDisplayCall) {
  ASSERT_NE(getDisplay(m_design), nullptr)
      << "$display SysFuncCall not found as direct Initial stmt";
}

TEST_F(SystemFunctions, DisplayCallName) {
  const uhdm::SysFuncCall *const c = getDisplay(m_design);
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getName(), "$display");
}

TEST_F(SystemFunctions, DisplayCallHasOneArgument) {
  const uhdm::SysFuncCall *const c = getDisplay(m_design);
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getArguments(), nullptr);
  EXPECT_EQ(c->getArguments()->size(), 1u);
}

TEST_F(SystemFunctions, ArgumentIsStringConstant) {
  const uhdm::SysFuncCall *const c = getDisplay(m_design);
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getArguments(), nullptr);
  ASSERT_EQ(c->getArguments()->size(), 1u);

  const uhdm::Constant *const arg =
      any_cast<uhdm::Constant>((*c->getArguments())[0]);
  ASSERT_NE(arg, nullptr) << "argument should be a Constant";
  // vpiStringConst = 6
  EXPECT_EQ(arg->getConstType(), 6) << "argument should have string const type";
}

TEST_F(SystemFunctions, ArgumentValueIsHelloWorld) {
  const uhdm::SysFuncCall *const c = getDisplay(m_design);
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getArguments(), nullptr);
  ASSERT_EQ(c->getArguments()->size(), 1u);

  const uhdm::Constant *const arg =
      any_cast<uhdm::Constant>((*c->getArguments())[0]);
  ASSERT_NE(arg, nullptr);
  // getValue() returns the raw string without surrounding quotes
  EXPECT_EQ(arg->getValue(), "hello world");
}

}  // namespace SURELOG

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
