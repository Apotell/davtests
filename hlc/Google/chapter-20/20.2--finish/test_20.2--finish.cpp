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

// Tests for 20.2--finish.sv (tags: 20.2)
//   module top();
//   initial
//   	$finish;
//   endmodule
//
// $finish (tags: 20.2) is a no-argument simulation control task. There is no
// "begin ... end" in the source, so the Initial process's statement is the
// "$finish" SysTaskCall directly, with no Begin wrapper in between. Called
// without parentheses, it is modeled as a property (vpiIsProperty), the
// same way a parenthesis-less "$random" is.
//
// Checked:
//   - design has module "top" with exactly 1 process, and it is an Initial
//   - the Initial's statement is a SysTaskCall named "$finish" directly (no
//     Begin wrapper, since there is no "begin ... end" in the source)
//   - the "$finish" SysTaskCall has no argument list (called without
//     parentheses) and is flagged as a property (vpiIsProperty)
//   - compiler reports zero errors
//
// NOT CHECKED: runtime effects (whether $finish actually terminates
// simulation) cannot be observed -- HLC is a compiler/elaborator with no
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
#include <hldb/sys_task_call.h>
#include <hldb/vpi_user.h>

namespace hlc {

class FinishTaskTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "20.2--finish.hlc"}); }
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

  static const hldb::SysTaskCall *getFinishCall() {
    const hldb::Initial *const init = getInitialProcess();
    if (init == nullptr) {
      return nullptr;
    }
    return init->getStmt<hldb::SysTaskCall>();
  }
};

// --- module / initial process ------------------------------------------------

TEST_F(FinishTaskTest, ModuleExists) { EXPECT_NE(getModule(), nullptr); }

TEST_F(FinishTaskTest, ModuleHasOneInitialProcess) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getProcesses(), nullptr);
  EXPECT_EQ(mod->getProcesses()->size(), 1u);
  EXPECT_NE(getInitialProcess(), nullptr);
}

// --- $finish; --------------------------------------------------------------------

TEST_F(FinishTaskTest, InitialStmtIsFinishSysTaskCall) {
  const hldb::SysTaskCall *const call = getFinishCall();
  ASSERT_NE(call, nullptr) << "'$finish;' should be the Initial's statement directly, with no Begin wrapper";
  EXPECT_EQ(call->getName(), "$finish");
}

TEST_F(FinishTaskTest, FinishCallHasNoArgumentsAndIsProperty) {
  const hldb::SysTaskCall *const call = getFinishCall();
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getArguments(), nullptr) << "'$finish' called without parentheses or arguments";
  EXPECT_TRUE(call->getIsProperty()) << "20.2: a parenthesis-less '$finish' is modeled as a property";
}

// --- compiler diagnostics -----------------------------------------------------

TEST_F(FinishTaskTest, CompilesWithNoErrors) { EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND), nullptr); }

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
