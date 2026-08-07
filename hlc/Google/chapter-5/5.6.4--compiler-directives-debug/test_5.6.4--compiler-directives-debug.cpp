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

// Validates that the `__FILE__ and `__LINE__ compiler directives are expanded
// by the preprocessor and appear in UHDM as resolved constant values.
//
// SV source (line 17):
//   initial $display("At %s @ %d\n", `__FILE__, `__LINE__);
//
// UHDM structure:
//   Module name:directives
//     Initial
//       vpiStmt: SysTaskCall "$display"
//         vpiArgument[0]: Constant (string, 6)  -- format string "At %s @ %d\n"
//         vpiArgument[1]: Constant (string, 6)  -- `__FILE__ -> full source path
//         vpiArgument[2]: Constant (uint,   9)  -- `__LINE__ -> integer 17
//
// Key assertions:
//   - `__FILE__ expands to a string constant containing the source filename.
//   - `__LINE__ expands to the integer constant at the line of the directive.

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

class CompilerDirectivesDebug : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "5.6.4--compiler-directives-debug.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

static const hldb::SysTaskCall *getDisplay(const hldb::Design *d) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("directives", d->getAllModules());
  if (!top || !top->getProcesses()) return nullptr;
  for (const hldb::Process *const p : *top->getProcesses()) {
    if (const hldb::Initial *const i = any_cast<hldb::Initial>(p)) return i->getStmt<hldb::SysTaskCall>();
  }
  return nullptr;
}

static const hldb::Constant *getArg(const hldb::Design *d, std::size_t idx) {
  const hldb::SysTaskCall *const c = getDisplay(d);
  if (!c || !c->getArguments() || c->getArguments()->size() <= idx) return nullptr;
  return any_cast<hldb::Constant>((*c->getArguments())[idx]);
}

// ----
// Module
// ----
TEST_F(CompilerDirectivesDebug, ModuleExists) {
  ASSERT_NE(hldb::findByName<hldb::Module>("directives", m_design->getAllModules()), nullptr)
      << "module 'directives' not found";
}

// ----
// $display with three arguments
// ----
TEST_F(CompilerDirectivesDebug, DisplayCallHasThreeArguments) {
  const hldb::SysTaskCall *const c = getDisplay(m_design);
  ASSERT_NE(c, nullptr) << "$display call not found";
  ASSERT_NE(c->getArguments(), nullptr);
  EXPECT_EQ(c->getArguments()->size(), 3u);
}

// ----
// Argument 0: format string "At %s @ %d\n"
// ----
TEST_F(CompilerDirectivesDebug, FormatStringIsStringConstant) {
  const hldb::Constant *const arg = getArg(m_design, 0);
  ASSERT_NE(arg, nullptr) << "argument 0 not found or not a Constant";
  // vpiStringConst = 6
  EXPECT_EQ(arg->getConstType(), 6);
}

TEST_F(CompilerDirectivesDebug, FormatStringValue) {
  const hldb::Constant *const arg = getArg(m_design, 0);
  ASSERT_NE(arg, nullptr);
  // vpiValue stores the raw escape sequence -- literal backslash-n, not a newline.
  EXPECT_EQ(arg->getValue(), "At %s @ %d\\n");
}

// ----
// Argument 1: `__FILE__ -- preprocessor expands to the source file path
// ----
TEST_F(CompilerDirectivesDebug, FileDirectiveIsStringConstant) {
  const hldb::Constant *const arg = getArg(m_design, 1);
  ASSERT_NE(arg, nullptr) << "`__FILE__ argument not found or not a Constant";
  // vpiStringConst = 6
  EXPECT_EQ(arg->getConstType(), 6);
}

TEST_F(CompilerDirectivesDebug, FileDirectiveContainsSourceFilename) {
  // The full path is installation-dependent; check the basename only.
  const hldb::Constant *const arg = getArg(m_design, 1);
  ASSERT_NE(arg, nullptr);
  const std::string_view val = arg->getValue();
  EXPECT_NE(val.find("5.6.4--compiler-directives-debug.sv"), std::string_view::npos)
      << "`__FILE__ should expand to a path containing the source filename; "
         "got: "
      << val;
}

// ----
// Argument 2: `__LINE__ -- preprocessor expands to the integer line number
// ----
TEST_F(CompilerDirectivesDebug, LineDirectiveIsIntegerConstant) {
  const hldb::Constant *const arg = getArg(m_design, 2);
  ASSERT_NE(arg, nullptr) << "`__LINE__ argument not found or not a Constant";
  // vpiConstType: unsigned int (9)
  EXPECT_EQ(arg->getConstType(), 9);
}

TEST_F(CompilerDirectivesDebug, LineDirectiveValueIsSeventeen) {
  // The `__LINE__ directive is on line 17 of the SV source.
  const hldb::Constant *const arg = getArg(m_design, 2);
  ASSERT_NE(arg, nullptr);
  EXPECT_EQ(arg->getValue(), "17");
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
