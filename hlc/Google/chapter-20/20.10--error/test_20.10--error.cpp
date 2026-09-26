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

// Tests for 20.10--error.sv (tags: 20.10)
//   module top();
//   initial begin
//   	$error("error");
//   end
//   endmodule
//
// IEEE 1800-2023 Sec 20.10, "Severity system tasks":
// "$error [ ( [ list_of_arguments ] ) ] ;" -- "$error shall generate a
// run-time error." Unlike $fatal, $error takes no finish_number argument,
// so its call here has exactly one argument: the message string, using the
// same syntax as $display (21.2.1).
//
// Checked:
//   - design has module "top" with exactly 1 process, and it is an Initial
//   - the Initial's body is a Begin (from the explicit "begin ... end")
//     wrapping exactly 1 statement
//   - that statement is a SysTaskCall named "$error" with exactly 1
//     argument: a Constant string "error" (vpiStringConst)
//   - compiler reports zero errors
//
// NOT CHECKED: runtime effects (that $error actually reports a run-time
// error and lets elaboration continue, rather than aborting like $fatal)
// cannot be observed -- HLC is a compiler/elaborator with no simulation
// capability, so no execution ever happens for this test to check, matching
// the .hlc file's own note that this is a parsing, not simulation, test.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/initial.h>
#include <hldb/module.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/sys_task_call.h>
#include <hldb/vpi_user.h>

namespace hlc {

class ErrorTaskTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "20.10--error.hlc"}); }
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

  static const hldb::Begin *getInitialBody() {
    const hldb::Initial *const init = getInitialProcess();
    if (init == nullptr) {
      return nullptr;
    }
    return init->getStmt<hldb::Begin>();
  }

  static const hldb::SysTaskCall *getErrorCall() {
    const hldb::Begin *const body = getInitialBody();
    if (body == nullptr || body->getStmts() == nullptr || body->getStmts()->empty()) {
      return nullptr;
    }
    return any_cast<hldb::SysTaskCall>(body->getStmts()->at(0));
  }
};

// --- module / initial process ------------------------------------------------

TEST_F(ErrorTaskTest, ModuleExists) { EXPECT_NE(getModule(), nullptr); }

TEST_F(ErrorTaskTest, ModuleHasOneInitialProcess) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getProcesses(), nullptr);
  EXPECT_EQ(mod->getProcesses()->size(), 1u);
  EXPECT_NE(getInitialProcess(), nullptr);
}

TEST_F(ErrorTaskTest, InitialBodyIsBeginWithOneStmt) {
  const hldb::Begin *const body = getInitialBody();
  ASSERT_NE(body, nullptr) << "'initial begin ... end' should wrap the body in a Begin";
  ASSERT_NE(body->getStmts(), nullptr);
  EXPECT_EQ(body->getStmts()->size(), 1u) << "'$error(\"error\");' is the only statement";
}

// --- $error("error"); ---------------------------------------------------------

TEST_F(ErrorTaskTest, StmtIsErrorSysTaskCall) {
  const hldb::SysTaskCall *const call = getErrorCall();
  ASSERT_NE(call, nullptr) << "'$error(...)' should be a SysTaskCall";
  EXPECT_EQ(call->getName(), "$error");
}

TEST_F(ErrorTaskTest, ErrorCallHasOneStringArgument) {
  const hldb::SysTaskCall *const call = getErrorCall();
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 1u)
      << "20.10: '$error' takes no finish_number, unlike '$fatal' -- only the message argument";

  const hldb::Constant *const msg = any_cast<hldb::Constant>(call->getArguments()->at(0));
  ASSERT_NE(msg, nullptr) << "'\"error\"' should be a Constant";
  EXPECT_EQ(msg->getConstType(), vpiStringConst);
  EXPECT_EQ(msg->getDecompile(), "\"error\"");
}

// --- compiler diagnostics -----------------------------------------------------

// $error is itself a diagnostic-reporting construct (20.10: "shall generate
// a run-time error"), not one that should trigger any compile-time
// diagnostic on its own when used correctly with a bare string literal.
TEST_F(ErrorTaskTest, CompilesWithNoErrors) { EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND), nullptr); }

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
