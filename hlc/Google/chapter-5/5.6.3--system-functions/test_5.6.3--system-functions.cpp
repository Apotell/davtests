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
//   Module name:systemfn
//     Initial
//       vpiStmt: SysTaskCall "$display"   ← direct stmt, NO Begin wrapper
//         vpiArgument[0]: Constant (vpiStringConst=6), getValue()="hello world"
//
// Notable difference from the builtin-methods tests: the SysTaskCall is the
// direct statement of the Initial block, not wrapped in a Begin.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/initial.h>
#include <hldb/module.h>
#include <hldb/sys_func_call.h>

namespace hlc {

class SystemFunctions : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "5.6.3--system-functions.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

static const hldb::SysTaskCall *getDisplay(const hldb::Design *d) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("systemfn", d->getAllModules());
  if (!top || !top->getProcesses()) return nullptr;
  for (const hldb::Process *const p : *top->getProcesses()) {
    if (const hldb::Initial *const i = any_cast<hldb::Initial>(p)) return i->getStmt<hldb::SysTaskCall>();
  }
  return nullptr;
}

// ---------------------------------------------------------------------------
// Module
// ---------------------------------------------------------------------------
TEST_F(SystemFunctions, ModuleExists) {
  ASSERT_NE(hldb::findByName<hldb::Module>("systemfn", m_design->getAllModules()), nullptr)
      << "module 'systemfn' not found";
}

// ---------------------------------------------------------------------------
// initial $display("hello world")
// ---------------------------------------------------------------------------
TEST_F(SystemFunctions, InitialHasDisplayCall) {
  ASSERT_NE(getDisplay(m_design), nullptr) << "$display SysTaskCall not found as direct Initial stmt";
}

TEST_F(SystemFunctions, DisplayCallName) {
  const hldb::SysTaskCall *const c = getDisplay(m_design);
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getName(), "$display");
}

TEST_F(SystemFunctions, DisplayCallHasOneArgument) {
  const hldb::SysTaskCall *const c = getDisplay(m_design);
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getArguments(), nullptr);
  EXPECT_EQ(c->getArguments()->size(), 1u);
}

TEST_F(SystemFunctions, ArgumentIsStringConstant) {
  const hldb::SysTaskCall *const c = getDisplay(m_design);
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getArguments(), nullptr);
  ASSERT_EQ(c->getArguments()->size(), 1u);

  const hldb::Constant *const arg = any_cast<hldb::Constant>((*c->getArguments())[0]);
  ASSERT_NE(arg, nullptr) << "argument should be a Constant";
  // vpiStringConst = 6
  EXPECT_EQ(arg->getConstType(), 6) << "argument should have string const type";
}

TEST_F(SystemFunctions, ArgumentValueIsHelloWorld) {
  const hldb::SysTaskCall *const c = getDisplay(m_design);
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getArguments(), nullptr);
  ASSERT_EQ(c->getArguments()->size(), 1u);

  const hldb::Constant *const arg = any_cast<hldb::Constant>((*c->getArguments())[0]);
  ASSERT_NE(arg, nullptr);
  // getValue() returns the raw string without surrounding quotes
  EXPECT_EQ(arg->getValue(), "hello world");
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
