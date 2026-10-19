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

// Tests for 20.4--printtimescale.sv (tags: 20.4)
//   `timescale 1 ms / 1 us
//   module top();
//   initial
//   	$printtimescale;
//   endmodule
//
// IEEE 1800-2023 Sec 20.4.1, "$printtimescale": "$printtimescale [ (
// hierarchical_identifier ) ] ;" -- the argument is optional; with no
// argument, the time unit and precision of the current module are
// displayed. Since there is no "begin ... end" in the source, the initial
// procedure's statement (Sec 9.2.1) is the "$printtimescale" call itself.
//
// Sec 22.7: "`timescale 1 ms / 1 us" sets the time unit and precision of
// the module that follows, overriding the .hlc file's "-timescale=1ns/1ns"
// default. Sec 37 (VPI) encodes vpiTimeUnit / vpiTimePrecision as the
// power-of-10 exponent of the value in seconds:
//   1 ms = 10^-3 -> time unit      = -3
//   1 us = 10^-6 -> time precision = -6
//
// Checked:
//   - design has module "top" with exactly 1 process, and it is an Initial
//   - "top"'s time unit is -3 (1 ms) and time precision is -6 (1 us)
//   - the Initial's statement is a SysTaskCall named "$printtimescale"
//     directly (no Begin wrapper, since there is no "begin ... end" in the
//     source)
//   - the "$printtimescale" SysTaskCall has no arguments
//   - compiler reports zero fatal, syntax and error diagnostics; this also
//     covers "endmodule", which has no HLDB node of its own
//
// NOT CHECKED:
//   - runtime effects (the timescale text that $printtimescale prints) --
//     HLC is a compiler/elaborator with no simulation capability; the .sv
//     header tags this test as "simulation parsing".
//   - the SourceFile's time unit / precision -- Sec 22.7 applies
//     "`timescale" to design elements; a per-file timescale is an HLDB
//     detail, not something the LRM defines.
//   - vpiIsProperty on the parenthesis-less "$printtimescale" -- that is an
//     HLDB encoding, not something Sec 20.4 dictates.

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

class PrintTimescaleTaskTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "20.4--printtimescale.hlc"}); }
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

  static const hldb::SysTaskCall *getPrintTimescaleCall() {
    const hldb::Initial *const init = getInitialProcess();
    if (init == nullptr) {
      return nullptr;
    }
    return init->getStmt<hldb::SysTaskCall>();
  }
};

// --- module / initial process ------------------------------------------------

TEST_F(PrintTimescaleTaskTest, ModuleExists) { EXPECT_NE(getModule(), nullptr); }

TEST_F(PrintTimescaleTaskTest, ModuleHasOneInitialProcess) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getProcesses(), nullptr);
  EXPECT_EQ(mod->getProcesses()->size(), 1u);
  EXPECT_NE(getInitialProcess(), nullptr);
}

// --- `timescale 1 ms / 1 us ---------------------------------------------------

TEST_F(PrintTimescaleTaskTest, ModuleTimeUnitIsMillisecond) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  EXPECT_EQ(mod->getTimeUnit(), -3) << "22.7: time unit should be -3 (1 ms = 10^-3 s)";
}

TEST_F(PrintTimescaleTaskTest, ModuleTimePrecisionIsMicrosecond) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  EXPECT_EQ(mod->getTimePrecision(), -6) << "22.7: time precision should be -6 (1 us = 10^-6 s)";
}

// --- $printtimescale; ----------------------------------------------------------

TEST_F(PrintTimescaleTaskTest, InitialStmtIsPrintTimescaleSysTaskCall) {
  const hldb::SysTaskCall *const call = getPrintTimescaleCall();
  ASSERT_NE(call, nullptr) << "'$printtimescale;' should be the Initial's statement directly, with no Begin wrapper";
  EXPECT_EQ(call->getName(), "$printtimescale");
}

TEST_F(PrintTimescaleTaskTest, PrintTimescaleCallHasNoArguments) {
  const hldb::SysTaskCall *const call = getPrintTimescaleCall();
  ASSERT_NE(call, nullptr);
  EXPECT_TRUE(call->getArguments() == nullptr || call->getArguments()->empty())
      << "20.4.1: '$printtimescale' is called here without the optional hierarchical_identifier";
}

// --- compiler diagnostics -----------------------------------------------------

TEST_F(PrintTimescaleTaskTest, CompilesWithNoErrors) {
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
