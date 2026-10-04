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

// Tests for 20.3--time.sv (tags: 20.3)
//   module top();
//   initial
//   	$display($time);
//   endmodule
//
// IEEE 1800-2023 Sec 20.3, "Simulation time functions": "function time
// $time;" -- returns the current simulation time as a 64-bit unsigned
// integer, scaled to the local time unit, and takes no arguments.
//
// $time is called here with no parentheses, and -- like "$realtime"
// (tested in 20.3--realtime) but unlike "$stime" (tested in 20.3--stime,
// which IS flagged a property) -- it is NOT flagged as a property
// (vpiIsProperty) in the HLDB. These three argument-less time functions do
// not all agree on this point, so each is verified independently rather
// than assumed.
//
// There is no "begin ... end" in the source, so the Initial process's
// statement is the "$display" SysTaskCall directly, with no Begin wrapper
// in between.
//
// Checked:
//   - design has module "top" with exactly 1 process, and it is an Initial
//   - the Initial's statement is a SysTaskCall named "$display" directly
//     (no Begin wrapper, since there is no "begin ... end" in the source)
//     with exactly 1 argument: a SysFuncCall named "$time"
//   - the "$time" SysFuncCall has no argument list and is NOT flagged as a
//     property (vpiIsProperty)
//   - compiler reports zero errors
//
// NOT CHECKED: runtime effects (the actual simulation time value returned
// by $time) cannot be observed -- HLC is a compiler/elaborator with no
// simulation capability, so no execution ever happens for this test to
// check.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/initial.h>
#include <hldb/module.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/sys_func_call.h>
#include <hldb/sys_task_call.h>
#include <hldb/vpi_user.h>

namespace hlc {

class TimeFunctionTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "20.3--time.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getModule() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::Initial *getInitialProcess() {
    const hldb::Module *const mod = getModule();
    if (mod == nullptr || mod->getProcesses() == nullptr || mod->getProcesses()->empty()) {
      return nullptr;
    }
    return any_cast<hldb::Initial>(mod->getProcesses()->at(0));
  }

  static const hldb::SysTaskCall *getDisplayCall() {
    const hldb::Initial *const init = getInitialProcess();
    if (init == nullptr) {
      return nullptr;
    }
    return init->getStmt<hldb::SysTaskCall>();
  }

  static const hldb::SysFuncCall *getTimeCall() {
    const hldb::SysTaskCall *const display = getDisplayCall();
    if (display == nullptr || display->getArguments() == nullptr || display->getArguments()->empty()) {
      return nullptr;
    }
    return any_cast<hldb::SysFuncCall>(display->getArguments()->at(0));
  }
};

// --- module / initial process ------------------------------------------------

TEST_F(TimeFunctionTest, ModuleExists) { EXPECT_NE(getModule(), nullptr); }

TEST_F(TimeFunctionTest, ModuleHasOneInitialProcess) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getProcesses(), nullptr);
  EXPECT_EQ(mod->getProcesses()->size(), 1u);
  EXPECT_NE(getInitialProcess(), nullptr);
}

// --- $display($time); --------------------------------------------------------

TEST_F(TimeFunctionTest, InitialStmtIsDisplaySysTaskCall) {
  const hldb::SysTaskCall *const call = getDisplayCall();
  ASSERT_NE(call, nullptr) << "'$display(...);' should be the Initial's statement directly, with no Begin wrapper";
  EXPECT_EQ(call->getName(), "$display");
}

TEST_F(TimeFunctionTest, DisplayCallHasOneTimeArgument) {
  const hldb::SysTaskCall *const call = getDisplayCall();
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 1u);

  EXPECT_NE(getTimeCall(), nullptr) << "'$time' should be a SysFuncCall";
}

TEST_F(TimeFunctionTest, TimeCallIsNamedCorrectly) {
  const hldb::SysFuncCall *const call = getTimeCall();
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getName(), "$time");
}

TEST_F(TimeFunctionTest, TimeCallHasNoArgumentsAndIsNotProperty) {
  const hldb::SysFuncCall *const call = getTimeCall();
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getArguments(), nullptr) << "'$time' never takes an argument list";
  EXPECT_FALSE(call->getIsProperty())
      << "20.3: unlike a parenthesis-less '$stime', '$time' is not flagged as a property";
}

// --- compiler diagnostics -----------------------------------------------------

TEST_F(TimeFunctionTest, CompilesWithNoErrors) { EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND), nullptr); }

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
