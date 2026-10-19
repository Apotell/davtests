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
// IEEE 1800-2023 Sec 20.4.2, "$timeformat": "$timeformat [ ( units_number ,
// precision_number , suffix_string , minimum_field_width ) ] ;" -- here
// units_number -9 (ns), precision 5, suffix "ns" and minimum field width
// 10, which set how "%t" formats times (Sec 21.2.1.3).
//
// Sec 5.7.1: an integer literal carries no sign, so "-9" is the unary minus
// operator (Sec 11.4.3) applied to the literal 9.
//
// Sec 22.7: "`timescale 1 fs / 1 fs" sets the module's time unit and
// precision; Sec 37 (VPI) encodes both as the power-of-10 exponent of the
// value in seconds: 1 fs = 10^-15 -> -15 / -15.
//
// Checked:
//   - design has module "top" with exactly 1 process, and it is an Initial
//   - "top"'s time unit and precision are both -15 (1 fs)
//   - the Initial's body is a Begin (from the explicit "begin ... end")
//     declaring no variables and wrapping exactly 2 statements, in order
//   - statement 1 is a SysTaskCall "$timeformat" with exactly 4 arguments:
//     a unary minus Operation on Constant "9", then Constants "5", "ns"
//     (string) and "10"
//   - statement 2 is a SysTaskCall "$display" with exactly 2 arguments: a
//     string Constant "%t" and a SysFuncCall "$realtime" with no arguments
//   - compiler reports zero fatal, syntax and error diagnostics; this also
//     covers "endmodule", which has no HLDB node of its own
//
// Call arguments are read through NamedArgument::getHighConn(): this HLDB
// wraps every tf_call argument, positional ones included, in a NamedArgument.
//
// NOT CHECKED:
//   - runtime effects (the formatted time text printed by "%t") -- HLC is a
//     compiler/elaborator with no simulation capability; the .sv header
//     tags this test as "simulation parsing".
//   - the SourceFile's time unit / precision -- Sec 22.7 applies
//     "`timescale" to design elements; a per-file timescale is an HLDB
//     detail, not something the LRM defines.
//   - the vpiConstType of the unsized literals 9, 5 and 10, and
//     vpiIsProperty on "$realtime" -- HLDB encodings, not LRM.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/initial.h>
#include <hldb/module.h>
#include <hldb/named_argument.h>
#include <hldb/operation.h>
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
  // Every tf_call argument is wrapped in a NamedArgument; the actual
  // argument expression is its high conn.
  template <typename T>
  static const T *getArgument(const hldb::NamedArgumentCollection *args, size_t index) {
    if (args == nullptr || args->size() <= index) {
      return nullptr;
    }
    const hldb::NamedArgument *const arg = args->at(index);
    if (arg == nullptr) {
      return nullptr;
    }
    return arg->getHighConn<T>();
  }

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

  static const hldb::SysTaskCall *getStmt(size_t index) {
    const hldb::Begin *const body = getInitialBody();
    if (body == nullptr || body->getStmts() == nullptr || body->getStmts()->size() <= index) {
      return nullptr;
    }
    return any_cast<hldb::SysTaskCall>(body->getStmts()->at(index));
  }

  static const hldb::SysTaskCall *getTimeformatCall() { return getStmt(0); }

  static const hldb::SysTaskCall *getDisplayCall() { return getStmt(1); }

  static const hldb::SysFuncCall *getRealtimeCall() {
    const hldb::SysTaskCall *const display = getDisplayCall();
    if (display == nullptr) {
      return nullptr;
    }
    return getArgument<hldb::SysFuncCall>(display->getArguments(), 1);
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
  EXPECT_TRUE(body->getVariables() == nullptr || body->getVariables()->empty())
      << "no variable is declared in this body";
  ASSERT_NE(body->getStmts(), nullptr);
  EXPECT_EQ(body->getStmts()->size(), 2u) << "'$timeformat(...);' and '$display(...);' are the only statements";
}

// --- `timescale 1 fs / 1 fs ---------------------------------------------------

TEST_F(TimeformatTaskTest, ModuleTimeUnitAndPrecisionAreFemtosecond) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  EXPECT_EQ(mod->getTimeUnit(), -15) << "22.7: time unit should be -15 (1 fs = 10^-15 s)";
  EXPECT_EQ(mod->getTimePrecision(), -15) << "22.7: time precision should be -15 (1 fs = 10^-15 s)";
}

// --- $timeformat(-9, 5, "ns", 10); --------------------------------------------

TEST_F(TimeformatTaskTest, FirstStmtIsTimeformatSysTaskCall) {
  const hldb::SysTaskCall *const call = getTimeformatCall();
  ASSERT_NE(call, nullptr) << "'$timeformat(...)' should be a SysTaskCall";
  EXPECT_EQ(call->getName(), "$timeformat");
}

TEST_F(TimeformatTaskTest, TimeformatCallHasFourArgumentsInOrder) {
  const hldb::SysTaskCall *const call = getTimeformatCall();
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 4u)
      << "20.4.2: units_number, precision_number, suffix_string, minimum_field_width";

  const hldb::Operation *const unitsNumber = getArgument<hldb::Operation>(call->getArguments(), 0);
  ASSERT_NE(unitsNumber, nullptr) << "5.7.1: '-9' is the unary minus operator applied to the literal 9";
  EXPECT_EQ(unitsNumber->getOpType(), vpiMinusOp);
  ASSERT_NE(unitsNumber->getOperands(), nullptr);
  ASSERT_EQ(unitsNumber->getOperands()->size(), 1u);
  const hldb::Constant *const unitsNumberOperand = any_cast<hldb::Constant>(unitsNumber->getOperands()->at(0));
  ASSERT_NE(unitsNumberOperand, nullptr) << "'9' should be a Constant";
  EXPECT_EQ(unitsNumberOperand->getDecompile(), "9");

  const hldb::Constant *const precision = getArgument<hldb::Constant>(call->getArguments(), 1);
  ASSERT_NE(precision, nullptr) << "'5' should be a Constant";
  EXPECT_EQ(precision->getDecompile(), "5");

  const hldb::Constant *const suffix = getArgument<hldb::Constant>(call->getArguments(), 2);
  ASSERT_NE(suffix, nullptr) << "'\"ns\"' should be a Constant";
  EXPECT_EQ(suffix->getConstType(), vpiStringConst) << "'\"ns\"' is a string literal";
  EXPECT_EQ(suffix->getDecompile(), "\"ns\"");

  const hldb::Constant *const width = getArgument<hldb::Constant>(call->getArguments(), 3);
  ASSERT_NE(width, nullptr) << "'10' should be a Constant";
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

  const hldb::Constant *const fmt = getArgument<hldb::Constant>(call->getArguments(), 0);
  ASSERT_NE(fmt, nullptr) << "'\"%t\"' should be a Constant";
  EXPECT_EQ(fmt->getConstType(), vpiStringConst) << "'\"%t\"' is a string literal";
  EXPECT_EQ(fmt->getDecompile(), "\"%t\"");

  EXPECT_NE(getRealtimeCall(), nullptr) << "'$realtime' should be a SysFuncCall";
}

TEST_F(TimeformatTaskTest, RealtimeCallIsNamedCorrectlyWithNoArguments) {
  const hldb::SysFuncCall *const call = getRealtimeCall();
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getName(), "$realtime");
  EXPECT_TRUE(call->getArguments() == nullptr || call->getArguments()->empty())
      << "20.3: '$realtime' takes no arguments";
}

// --- compiler diagnostics -----------------------------------------------------

TEST_F(TimeformatTaskTest, CompilesWithNoErrors) {
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
