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
// $printtimescale (tags: 20.4) is a no-argument simulation control task,
// the same shape as "$exit" / "$finish" / "$stop" (chapter 20.2): there is
// no "begin ... end" in the source, so the Initial process's statement is
// the "$printtimescale" SysTaskCall directly, with no Begin wrapper in
// between. Called without parentheses, it is modeled as a property
// (vpiIsProperty).
//
// The leading "`timescale 1 ms / 1 us" directive overrides the .hlc file's
// own "-timescale=1ns/1ns" command-line default. UHDM encodes time values
// as powers-of-10 exponents (SI notation):
//   1 ms = 10^-3 -> vpiTimeUnit      = -3
//   1 us = 10^-6 -> vpiTimePrecision = -6
// Both the SourceFile and the Module receive these same timescale values.
//
// Checked:
//   - design has module "top" with exactly 1 process, and it is an Initial
//   - the Initial's statement is a SysTaskCall named "$printtimescale"
//     directly (no Begin wrapper, since there is no "begin ... end" in the
//     source)
//   - the "$printtimescale" SysTaskCall has no argument list (called
//     without parentheses) and is flagged as a property (vpiIsProperty)
//   - the Module's time unit is -3 (1 ms) and time precision is -6 (1 us)
//   - the SourceFile's time unit is -3 (1 ms) and time precision is -6
//     (1 us), the same values propagated to file scope
//   - compiler reports zero errors
//
// NOT CHECKED: runtime effects (the actual timescale banner text that
// $printtimescale would print) cannot be observed -- HLC is a
// compiler/elaborator with no simulation capability, so no execution ever
// happens for this test to check.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/initial.h>
#include <hldb/module.h>
#include <hldb/source_file.h>
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

  static const hldb::SourceFile *getSourceFile() {
    if (m_design->getSourceFiles() == nullptr || m_design->getSourceFiles()->empty()) {
      return nullptr;
    }
    return m_design->getSourceFiles()->at(0);
  }

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
  EXPECT_EQ(mod->getTimeUnit(), -3) << "time unit should be -3 (1 ms = 10^-3 s)";
}

TEST_F(PrintTimescaleTaskTest, ModuleTimePrecisionIsMicrosecond) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  EXPECT_EQ(mod->getTimePrecision(), -6) << "time precision should be -6 (1 us = 10^-6 s)";
}

TEST_F(PrintTimescaleTaskTest, SourceFileTimeUnitIsMillisecond) {
  const hldb::SourceFile *const sf = getSourceFile();
  ASSERT_NE(sf, nullptr);
  EXPECT_EQ(sf->getTimeUnit(), -3) << "source file time unit should be -3 (1 ms = 10^-3 s)";
}

TEST_F(PrintTimescaleTaskTest, SourceFileTimePrecisionIsMicrosecond) {
  const hldb::SourceFile *const sf = getSourceFile();
  ASSERT_NE(sf, nullptr);
  EXPECT_EQ(sf->getTimePrecision(), -6) << "source file time precision should be -6 (1 us = 10^-6 s)";
}

// --- $printtimescale; ----------------------------------------------------------

TEST_F(PrintTimescaleTaskTest, InitialStmtIsPrintTimescaleSysTaskCall) {
  const hldb::SysTaskCall *const call = getPrintTimescaleCall();
  ASSERT_NE(call, nullptr) << "'$printtimescale;' should be the Initial's statement directly, with no Begin wrapper";
  EXPECT_EQ(call->getName(), "$printtimescale");
}

TEST_F(PrintTimescaleTaskTest, PrintTimescaleCallHasNoArgumentsAndIsProperty) {
  const hldb::SysTaskCall *const call = getPrintTimescaleCall();
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getArguments(), nullptr) << "'$printtimescale' called without parentheses or arguments";
  EXPECT_TRUE(call->getIsProperty()) << "20.4: a parenthesis-less '$printtimescale' is modeled as a property";
}

// --- compiler diagnostics -----------------------------------------------------

TEST_F(PrintTimescaleTaskTest, CompilesWithNoErrors) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
