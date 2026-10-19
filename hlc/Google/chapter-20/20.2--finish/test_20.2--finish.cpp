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
// IEEE 1800-2023 Sec 20.2, "Simulation control system tasks", Syntax 20-1:
// "simulation_control_task ::= ... | $finish [ ( n ) ] ; | ..." -- the
// diagnostic level argument n is optional; when it is omitted, a value of 1
// is taken as the default. Since there is no "begin ... end" in the
// source, the initial procedure's statement (Sec 9.2.1) is the "$finish"
// call itself.
//
// Checked:
//   - design has module "top" with exactly 1 process, and it is an Initial
//   - the Initial's statement is a SysTaskCall named "$finish" directly (no
//     Begin wrapper, since there is no "begin ... end" in the source)
//   - the "$finish" SysTaskCall has no arguments (n is omitted)
//   - compiler reports zero fatal, syntax and error diagnostics; this also
//     covers "endmodule", which has no HLDB node of its own -- leaving it
//     out is a syntax error (Sec 23.2.1)
//
// NOT CHECKED:
//   - runtime effects (simulation actually ending, and the default
//     diagnostic level 1 being used) -- HLC is a compiler/elaborator with no
//     simulation capability.
//   - vpiIsProperty on the parenthesis-less "$finish" -- that is an HLDB
//     encoding, not something Sec 20.2 dictates.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
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

// --- $finish; ------------------------------------------------------------------

TEST_F(FinishTaskTest, InitialStmtIsFinishSysTaskCall) {
  const hldb::SysTaskCall *const call = getFinishCall();
  ASSERT_NE(call, nullptr) << "'$finish;' should be the Initial's statement directly, with no Begin wrapper";
  EXPECT_EQ(call->getName(), "$finish");
}

TEST_F(FinishTaskTest, FinishCallHasNoArguments) {
  const hldb::SysTaskCall *const call = getFinishCall();
  ASSERT_NE(call, nullptr);
  EXPECT_TRUE(call->getArguments() == nullptr || call->getArguments()->empty())
      << "20.2: '$finish' is called here without the optional argument n";
}

// --- compiler diagnostics -----------------------------------------------------

TEST_F(FinishTaskTest, CompilesWithNoErrors) {
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0) << "'module top(); ... endmodule' should parse as one complete module";
  EXPECT_EQ(stats.nbError, 0) << "the source is legal SystemVerilog and should compile without errors";
}

}  // namespace hlc

#ifndef DAVTESTS_BULK_BUILD
int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
#endif
