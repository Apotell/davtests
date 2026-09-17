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

// Tests for 9.4.1--delay_control-sim.sv (tags: 9.4.1)
//   module top();
//      initial begin
//         $display(":assert: (0 == %d)", $time);
//         #10;
//         $display(":assert: (10 == %d)", $time);
//         #10;
//         $display(":assert: (20 == %d)", $time);
//         #10;
//         $display(":assert: (30 == %d)", $time);
//         $finish;
//      end
//   endmodule
//
// IEEE 1800-2017 Sec 9.4.1 "Delay control". Unlike 9.4.1--delay_control.sv
// (each "#10" directly precedes an assignment, so the delay and the
// statement it governs are one DelayControl node), this file's three
// "#10;" are bare delay controls terminated by their own semicolon --
// there is no statement left for them to delay. Confirmed against the
// AST/HLDB dump for this file: each such DelayControl carries only
// vpiParent and vpiDelay, with no vpiStmt entry at all, so getStmt() must
// be null. The 4 "$display" calls and the trailing "$finish" are separate
// SysTaskCall statements sitting directly in the initial block's Begin,
// interleaved with the 3 bare DelayControls -- 8 statements total.
//
// Each "$display" call's second argument is "$time", a call to the system
// function $time, not a variable reference, so it decompiles to a
// SysFuncCall node named "$time" rather than a RefObj.
//
// Checked:
//   - design has module "top" with no variables
//   - module has exactly 1 process, and it is an Initial
//   - the Initial's stmt is a Begin (begin/end wraps the whole body) with
//     exactly 8 statements
//   - stmts[0], [2], [4], [6]: SysTaskCall "$display", each with exactly 2
//     arguments: a Constant string ":assert: (N == %d)" and a SysFuncCall
//     "$time", for N = 0, 10, 20, 30 in source order
//   - stmts[1], [3], [5]: bare DelayControl, delay Constant "10"
//     (vpiIntConst), getStmt() is null (no statement is delayed -- the
//     delay terminates on its own ";")
//   - stmts[7]: SysTaskCall "$finish" with no arguments
//   - design-level typespecs (3): ModuleTypespec, StringTypespec,
//     IntTypespec (signed)
//   - compiler emits zero errors

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/delay_control.h>
#include <hldb/design.h>
#include <hldb/initial.h>
#include <hldb/module.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/sys_func_call.h>
#include <hldb/sys_task_call.h>
#include <hldb/vpi_user.h>

namespace hlc {

class DelayControlSimTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "9.4.1--delay_control-sim.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::Initial *getInitialProcess() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getProcesses() == nullptr || top->getProcesses()->empty()) {
      return nullptr;
    }
    return any_cast<hldb::Initial>(top->getProcesses()->at(0));
  }

  static const hldb::Begin *getInitialBody() {
    const hldb::Initial *const init = getInitialProcess();
    if (init == nullptr) {
      return nullptr;
    }
    return init->getStmt<hldb::Begin>();
  }

  static const hldb::SysTaskCall *getSysTaskCall(size_t index) {
    const hldb::Begin *const begin = getInitialBody();
    if (begin == nullptr || begin->getStmts() == nullptr || begin->getStmts()->size() <= index) {
      return nullptr;
    }
    return any_cast<hldb::SysTaskCall>(begin->getStmts()->at(index));
  }

  static const hldb::DelayControl *getDelayControl(size_t index) {
    const hldb::Begin *const begin = getInitialBody();
    if (begin == nullptr || begin->getStmts() == nullptr || begin->getStmts()->size() <= index) {
      return nullptr;
    }
    return any_cast<hldb::DelayControl>(begin->getStmts()->at(index));
  }

  static void ExpectDisplayAssertsTime(size_t index, std::string_view expectedMessage) {
    const hldb::SysTaskCall *const disp = getSysTaskCall(index);
    ASSERT_NE(disp, nullptr) << "9.4.1: stmt[" << index << "] should be a SysTaskCall";
    EXPECT_EQ(disp->getName(), "$display");
    ASSERT_NE(disp->getArguments(), nullptr);
    ASSERT_EQ(disp->getArguments()->size(), 2u);

    const hldb::Constant *const fmt = any_cast<hldb::Constant>(disp->getArguments()->at(0));
    ASSERT_NE(fmt, nullptr) << "first argument should be the format string";
    EXPECT_EQ(fmt->getValue(), expectedMessage);

    const hldb::SysFuncCall *const time = any_cast<hldb::SysFuncCall>(disp->getArguments()->at(1));
    ASSERT_NE(time, nullptr) << "'$time' should be a SysFuncCall, not a variable reference";
    EXPECT_EQ(time->getName(), "$time");
  }

  static void ExpectBareDelayControlOfTen(size_t index) {
    const hldb::DelayControl *const delay = getDelayControl(index);
    ASSERT_NE(delay, nullptr) << "9.4.1: stmt[" << index << "] should be a DelayControl";

    const hldb::Constant *const amount = delay->getDelay<hldb::Constant>();
    ASSERT_NE(amount, nullptr) << "'#10' must carry a delay expression";
    EXPECT_EQ(amount->getDecompile(), "10");
    EXPECT_EQ(amount->getConstType(), vpiIntConst) << "plain decimal delay -> constType int (7)";

    EXPECT_EQ(delay->getStmt(), nullptr) << "'#10;' has no statement of its own to delay";
  }
};

// --- module ----------------------------------------------------------------

TEST_F(DelayControlSimTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(DelayControlSimTest, ModuleHasNoVariables) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getVariables(), nullptr);
}

// --- initial process / begin-block structure --------------------------------

TEST_F(DelayControlSimTest, ModuleHasOneProcess) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  EXPECT_EQ(top->getProcesses()->size(), 1u);
}

TEST_F(DelayControlSimTest, TheOneProcessIsInitial) { EXPECT_NE(getInitialProcess(), nullptr); }

TEST_F(DelayControlSimTest, InitialStmtIsDirectlyABegin) {
  EXPECT_NE(getInitialBody(), nullptr) << "9.4.1: 'begin...end' must be modeled as a Begin";
}

TEST_F(DelayControlSimTest, BeginHasEightStmts) {
  const hldb::Begin *const begin = getInitialBody();
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  EXPECT_EQ(begin->getStmts()->size(), 8u) << "4 '$display' + 3 bare '#10;' + 1 '$finish'";
}

// --- $display(":assert: (N == %d)", $time) pairs ----------------------------

TEST_F(DelayControlSimTest, FirstStmtDisplaysTimeZero) { ExpectDisplayAssertsTime(0, ":assert: (0 == %d)"); }
TEST_F(DelayControlSimTest, ThirdStmtDisplaysTimeTen) { ExpectDisplayAssertsTime(2, ":assert: (10 == %d)"); }
TEST_F(DelayControlSimTest, FifthStmtDisplaysTimeTwenty) { ExpectDisplayAssertsTime(4, ":assert: (20 == %d)"); }
TEST_F(DelayControlSimTest, SeventhStmtDisplaysTimeThirty) { ExpectDisplayAssertsTime(6, ":assert: (30 == %d)"); }

// --- bare "#10;" delay controls ----------------------------------------------

TEST_F(DelayControlSimTest, SecondStmtIsBareDelayOfTen) { ExpectBareDelayControlOfTen(1); }
TEST_F(DelayControlSimTest, FourthStmtIsBareDelayOfTen) { ExpectBareDelayControlOfTen(3); }
TEST_F(DelayControlSimTest, SixthStmtIsBareDelayOfTen) { ExpectBareDelayControlOfTen(5); }

// --- $finish -----------------------------------------------------------------

TEST_F(DelayControlSimTest, EighthStmtIsFinishWithNoArguments) {
  const hldb::SysTaskCall *const finish = getSysTaskCall(7);
  ASSERT_NE(finish, nullptr) << "9.4.1: stmt[7] should be a SysTaskCall";
  EXPECT_EQ(finish->getName(), "$finish");
  EXPECT_EQ(finish->getArguments(), nullptr);
}

// --- design-level typespecs / compiler diagnostics ----------------------------

TEST_F(DelayControlSimTest, DesignHasThreeTypespecs) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 3u);
}

TEST_F(DelayControlSimTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
  EXPECT_EQ(stats.nbWarning, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
