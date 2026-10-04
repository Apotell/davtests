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

// Tests for 20.4--timeformat.sv (tags: 20.4)
//   `timescale 1 fs / 1 fs
//   module top();
//   initial begin
//   	$timeformat(-9, 5, "ns", 10);
//   	$display("%t", $realtime);
//   end
//   endmodule
//
// "`timescale 1 fs / 1 fs" sets both time unit and time precision to
// 10^-15 (1 fs), so Module, SourceFile, and (since there is only this one
// directive) their values all agree: -15 / -15.
//
// $timeformat (tags: 20.4) is called here with 4 arguments: "-9" (the
// units_number), "5" (the precision), "\"ns\"" (the suffix_string), and
// "10" (the minimum field width). The "-9" literal is a unary minus
// applied to a positive literal, not itself a literal -- per the same
// pattern as "$unsigned(-4)" in chapter 11.7, HLC represents it as an
// Operation (vpiMinusOp) wrapping a Constant "9", not a folded Constant
// "-9".
//
// $display("%t", $realtime) -- $realtime is called here with no
// parentheses; as in 20.3--realtime, it is NOT flagged as a property
// (vpiIsProperty) in the HLDB.
//
// Checked:
//   - design has module "top" with exactly 1 process, and it is an Initial
//   - the Initial's body is a Begin (from the explicit "begin ... end")
//     declaring no variables and wrapping exactly 2 statements
//   - the first statement is a SysTaskCall named "$timeformat" with
//     exactly 4 arguments: an Operation (vpiMinusOp) wrapping a Constant
//     unsigned int "9", a Constant unsigned int "5", a Constant string
//     "ns", and a Constant unsigned int "10"
//   - the second statement is a SysTaskCall named "$display" with exactly
//     2 arguments: a Constant string "%t" and a SysFuncCall named
//     "$realtime"
//   - the "$realtime" SysFuncCall has no argument list and is NOT flagged
//     as a property (vpiIsProperty)
//   - the Module's (and SourceFile's) time unit and time precision are
//     both -15 (1 fs)
//   - compiler reports zero errors
//
// NOT CHECKED: runtime effects (the actual formatted time string that
// $display("%t", ...) would print, after $timeformat changes the default
// formatting) cannot be observed -- HLC is a compiler/elaborator with no
// simulation capability, so no execution ever happens for this test to
// check.

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
#include <hldb/operation.h>
#include <hldb/source_file.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/sys_func_call.h>
#include <hldb/sys_task_call.h>
#include <hldb/vpi_user.h>

namespace hlc {

class TimeformatTaskTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "20.4--timeformat.hlc"}); }
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

  static const hldb::Begin *getInitialBody() {
    const hldb::Initial *const init = getInitialProcess();
    if (init == nullptr) {
      return nullptr;
    }
    return init->getStmt<hldb::Begin>();
  }

  static const hldb::SysTaskCall *getTimeformatCall() {
    const hldb::Begin *const body = getInitialBody();
    if (body == nullptr || body->getStmts() == nullptr || body->getStmts()->empty()) {
      return nullptr;
    }
    return any_cast<hldb::SysTaskCall>(body->getStmts()->at(0));
  }

  static const hldb::SysTaskCall *getDisplayCall() {
    const hldb::Begin *const body = getInitialBody();
    if (body == nullptr || body->getStmts() == nullptr || body->getStmts()->size() < 2u) {
      return nullptr;
    }
    return any_cast<hldb::SysTaskCall>(body->getStmts()->at(1));
  }

  static const hldb::SysFuncCall *getRealtimeCall() {
    const hldb::SysTaskCall *const display = getDisplayCall();
    if (display == nullptr || display->getArguments() == nullptr || display->getArguments()->size() < 2u) {
      return nullptr;
    }
    return any_cast<hldb::SysFuncCall>(display->getArguments()->at(1));
  }
};

// --- module / initial process ------------------------------------------------

TEST_F(TimeformatTaskTest, ModuleExists) { EXPECT_NE(getModule(), nullptr); }

TEST_F(TimeformatTaskTest, ModuleHasOneInitialProcess) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getProcesses(), nullptr);
  EXPECT_EQ(mod->getProcesses()->size(), 1u);
  EXPECT_NE(getInitialProcess(), nullptr);
}

TEST_F(TimeformatTaskTest, InitialBodyIsBeginWithNoVariablesAndTwoStmts) {
  const hldb::Begin *const body = getInitialBody();
  ASSERT_NE(body, nullptr) << "'initial begin ... end' should wrap the body in a Begin";
  EXPECT_EQ(body->getVariables(), nullptr) << "no variable is declared in this body";
  ASSERT_NE(body->getStmts(), nullptr);
  EXPECT_EQ(body->getStmts()->size(), 2u) << "'$timeformat(...);' and '$display(...);' are the only statements";
}

// --- `timescale 1 fs / 1 fs ---------------------------------------------------

TEST_F(TimeformatTaskTest, ModuleTimeUnitAndPrecisionAreFemtosecond) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  EXPECT_EQ(mod->getTimeUnit(), -15) << "time unit should be -15 (1 fs = 10^-15 s)";
  EXPECT_EQ(mod->getTimePrecision(), -15) << "time precision should be -15 (1 fs = 10^-15 s)";
}

TEST_F(TimeformatTaskTest, SourceFileTimeUnitAndPrecisionAreFemtosecond) {
  const hldb::SourceFile *const sf = getSourceFile();
  ASSERT_NE(sf, nullptr);
  EXPECT_EQ(sf->getTimeUnit(), -15) << "source file time unit should be -15 (1 fs = 10^-15 s)";
  EXPECT_EQ(sf->getTimePrecision(), -15) << "source file time precision should be -15 (1 fs = 10^-15 s)";
}

// --- $timeformat(-9, 5, "ns", 10); --------------------------------------------

TEST_F(TimeformatTaskTest, FirstStmtIsTimeformatSysTaskCall) {
  const hldb::SysTaskCall *const call = getTimeformatCall();
  ASSERT_NE(call, nullptr) << "'$timeformat(...)' should be a SysTaskCall";
  EXPECT_EQ(call->getName(), "$timeformat");
}

TEST_F(TimeformatTaskTest, TimeformatCallHasFourArguments) {
  const hldb::SysTaskCall *const call = getTimeformatCall();
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 4u);

  const hldb::Operation *const unitsNumber = any_cast<hldb::Operation>(call->getArguments()->at(0));
  ASSERT_NE(unitsNumber, nullptr) << "'-9' should be an unfolded unary-minus Operation, not a plain Constant";
  EXPECT_EQ(unitsNumber->getOpType(), vpiMinusOp);
  ASSERT_NE(unitsNumber->getOperands(), nullptr);
  ASSERT_EQ(unitsNumber->getOperands()->size(), 1u);
  const hldb::Constant *const unitsNumberOperand = any_cast<hldb::Constant>(unitsNumber->getOperands()->at(0));
  ASSERT_NE(unitsNumberOperand, nullptr);
  EXPECT_EQ(unitsNumberOperand->getConstType(), vpiUIntConst);
  EXPECT_EQ(unitsNumberOperand->getDecompile(), "9");

  const hldb::Constant *const precision = any_cast<hldb::Constant>(call->getArguments()->at(1));
  ASSERT_NE(precision, nullptr) << "'5' should be a Constant";
  EXPECT_EQ(precision->getConstType(), vpiUIntConst);
  EXPECT_EQ(precision->getDecompile(), "5");

  const hldb::Constant *const suffix = any_cast<hldb::Constant>(call->getArguments()->at(2));
  ASSERT_NE(suffix, nullptr) << "'\"ns\"' should be a Constant";
  EXPECT_EQ(suffix->getConstType(), vpiStringConst);
  EXPECT_EQ(suffix->getDecompile(), "\"ns\"");

  const hldb::Constant *const width = any_cast<hldb::Constant>(call->getArguments()->at(3));
  ASSERT_NE(width, nullptr) << "'10' should be a Constant";
  EXPECT_EQ(width->getConstType(), vpiUIntConst);
  EXPECT_EQ(width->getDecompile(), "10");
}

// --- $display("%t", $realtime); -----------------------------------------------

TEST_F(TimeformatTaskTest, SecondStmtIsDisplaySysTaskCall) {
  const hldb::SysTaskCall *const call = getDisplayCall();
  ASSERT_NE(call, nullptr) << "'$display(...)' should be a SysTaskCall";
  EXPECT_EQ(call->getName(), "$display");
}

TEST_F(TimeformatTaskTest, DisplayCallHasFormatAndRealtimeArgument) {
  const hldb::SysTaskCall *const call = getDisplayCall();
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 2u);

  const hldb::Constant *const fmt = any_cast<hldb::Constant>(call->getArguments()->at(0));
  ASSERT_NE(fmt, nullptr) << "'\"%t\"' should be a Constant";
  EXPECT_EQ(fmt->getConstType(), vpiStringConst);
  EXPECT_EQ(fmt->getDecompile(), "\"%t\"");

  EXPECT_NE(getRealtimeCall(), nullptr) << "'$realtime' should be a SysFuncCall";
}

TEST_F(TimeformatTaskTest, RealtimeCallIsNamedCorrectly) {
  const hldb::SysFuncCall *const call = getRealtimeCall();
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getName(), "$realtime");
}

TEST_F(TimeformatTaskTest, RealtimeCallHasNoArgumentsAndIsNotProperty) {
  const hldb::SysFuncCall *const call = getRealtimeCall();
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getArguments(), nullptr) << "'$realtime' never takes an argument list";
  EXPECT_FALSE(call->getIsProperty()) << "20.3: a parenthesis-less '$realtime' is not flagged as a property";
}

// --- compiler diagnostics -----------------------------------------------------

TEST_F(TimeformatTaskTest, CompilesWithNoErrors) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
