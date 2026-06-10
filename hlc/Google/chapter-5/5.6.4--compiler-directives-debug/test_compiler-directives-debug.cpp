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
//   Module name:work@directives
//     Initial
//       vpiStmt: SysFuncCall "$display"
//         vpiArgument[0]: Constant (string, 6)  — format string "At %s @ %d\n"
//         vpiArgument[1]: Constant (string, 6)  — `__FILE__ → full source path
//         vpiArgument[2]: Constant (uint,   9)  — `__LINE__ → integer 17
//
// Key assertions:
//   - `__FILE__ expands to a string constant containing the source filename.
//   - `__LINE__ expands to the integer constant at the line of the directive.

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

class CompilerDirectivesDebug : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "5.6.4--compiler-directives-debug.hlc"});

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
  const uhdm::Module *const top = uhdm::findByName<uhdm::Module>(
      "work@directives", d->getAllModules());
  if (!top || !top->getProcesses()) return nullptr;
  for (const uhdm::Process *const p : *top->getProcesses()) {
    if (const uhdm::Initial *const i = any_cast<uhdm::Initial>(p))
      return i->getStmt<uhdm::SysFuncCall>();
  }
  return nullptr;
}

static const uhdm::Constant *getArg(const uhdm::Design *d, std::size_t idx) {
  const uhdm::SysFuncCall *const c = getDisplay(d);
  if (!c || !c->getArguments() || c->getArguments()->size() <= idx)
    return nullptr;
  return any_cast<uhdm::Constant>((*c->getArguments())[idx]);
}

// ---------------------------------------------------------------------------
// Module
// ---------------------------------------------------------------------------
TEST_F(CompilerDirectivesDebug, ModuleExists) {
  ASSERT_NE(
      uhdm::findByName<uhdm::Module>("work@directives", m_design->getAllModules()),
      nullptr)
      << "module 'work@directives' not found";
}

// ---------------------------------------------------------------------------
// $display with three arguments
// ---------------------------------------------------------------------------
TEST_F(CompilerDirectivesDebug, DisplayCallHasThreeArguments) {
  const uhdm::SysFuncCall *const c = getDisplay(m_design);
  ASSERT_NE(c, nullptr) << "$display call not found";
  ASSERT_NE(c->getArguments(), nullptr);
  EXPECT_EQ(c->getArguments()->size(), 3u);
}

// ---------------------------------------------------------------------------
// Argument 0: format string "At %s @ %d\n"
// ---------------------------------------------------------------------------
TEST_F(CompilerDirectivesDebug, FormatStringIsStringConstant) {
  const uhdm::Constant *const arg = getArg(m_design, 0);
  ASSERT_NE(arg, nullptr) << "argument 0 not found or not a Constant";
  // vpiStringConst = 6
  EXPECT_EQ(arg->getConstType(), 6);
}

TEST_F(CompilerDirectivesDebug, FormatStringValue) {
  const uhdm::Constant *const arg = getArg(m_design, 0);
  ASSERT_NE(arg, nullptr);
  // vpiValue stores the raw escape sequence — literal backslash-n, not a newline.
  EXPECT_EQ(arg->getValue(), "At %s @ %d\\n");
}

// ---------------------------------------------------------------------------
// Argument 1: `__FILE__ — preprocessor expands to the source file path
// ---------------------------------------------------------------------------
TEST_F(CompilerDirectivesDebug, FileDirectiveIsStringConstant) {
  const uhdm::Constant *const arg = getArg(m_design, 1);
  ASSERT_NE(arg, nullptr) << "`__FILE__ argument not found or not a Constant";
  // vpiStringConst = 6
  EXPECT_EQ(arg->getConstType(), 6);
}

TEST_F(CompilerDirectivesDebug, FileDirectiveContainsSourceFilename) {
  // The full path is installation-dependent; check the basename only.
  const uhdm::Constant *const arg = getArg(m_design, 1);
  ASSERT_NE(arg, nullptr);
  const std::string_view val = arg->getValue();
  EXPECT_NE(val.find("5.6.4--compiler-directives-debug.sv"), std::string_view::npos)
      << "`__FILE__ should expand to a path containing the source filename; "
         "got: " << val;
}

// ---------------------------------------------------------------------------
// Argument 2: `__LINE__ — preprocessor expands to the integer line number
// ---------------------------------------------------------------------------
TEST_F(CompilerDirectivesDebug, LineDirectiveIsIntegerConstant) {
  const uhdm::Constant *const arg = getArg(m_design, 2);
  ASSERT_NE(arg, nullptr) << "`__LINE__ argument not found or not a Constant";
  // vpiConstType: unsigned int (9)
  EXPECT_EQ(arg->getConstType(), 9);
}

TEST_F(CompilerDirectivesDebug, LineDirectiveValueIsSeventeen) {
  // The `__LINE__ directive is on line 17 of the SV source.
  const uhdm::Constant *const arg = getArg(m_design, 2);
  ASSERT_NE(arg, nullptr);
  EXPECT_EQ(arg->getValue(), "17");
}

}  // namespace SURELOG

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
