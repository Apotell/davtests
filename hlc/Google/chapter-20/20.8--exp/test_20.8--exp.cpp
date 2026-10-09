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

// Tests for 20.8--exp.sv (tags: 20.8)
//   module top();
//   initial begin
//   	$display(":assert: (%f > 2.718) and (%f < 2.719)", $exp(1), $exp(1));
//   end
//   endmodule
//
// IEEE 1800-2023 Sec 20.8.2, "Real math functions", Table 20-4:
// "$exp(x)" -- "Exponential". It takes exactly one argument.
//
// Checked:
//   - design has module "top" with exactly 1 process, and it is an Initial
//   - the Initial's body is a Begin (from the explicit "begin ... end")
//     wrapping exactly 1 statement and no variables
//   - that statement is a SysTaskCall named "$display" with exactly 3
//     arguments: a Constant string ":assert: (%f > 2.718) and (%f < 2.719)"
//     (size 304) followed by two SysFuncCalls named "$exp"
//   - each "$exp" SysFuncCall has exactly 1 argument: a Constant unsigned
//     int "1" (size 64) whose typespec resolves to an IntTypespec
//   - compiler reports zero errors
//
// NOT CHECKED: runtime effects (that $exp(1) actually yields e ~= 2.71828
// and that the ":assert:" comparisons hold) cannot be observed -- HLC is a
// compiler/elaborator with no simulation capability, so no execution ever
// happens for this test to check.

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
#include <hldb/int_typespec.h>
#include <hldb/module.h>
#include <hldb/ref_typespec.h>
#include <hldb/string_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/sys_func_call.h>
#include <hldb/sys_task_call.h>
#include <hldb/vpi_user.h>

namespace hlc {

class ExpFunctionTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "20.8--exp.hlc"}); }
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

  static const hldb::SysTaskCall *getDisplayCall() {
    const hldb::Begin *const body = getInitialBody();
    if (body == nullptr || body->getStmts() == nullptr || body->getStmts()->empty()) {
      return nullptr;
    }
    return any_cast<hldb::SysTaskCall>(body->getStmts()->at(0));
  }

  // argIndex is the position in the $display argument list (1 or 2).
  static const hldb::SysFuncCall *getExpCall(size_t argIndex) {
    const hldb::SysTaskCall *const display = getDisplayCall();
    if (display == nullptr || display->getArguments() == nullptr || display->getArguments()->size() <= argIndex) {
      return nullptr;
    }
    return any_cast<hldb::SysFuncCall>(display->getArguments()->at(argIndex));
  }

  // Both calls are "$exp(1)": one Constant unsigned int argument "1".
  static void expectExpCallOfOne(const hldb::SysFuncCall *call) {
    ASSERT_NE(call, nullptr) << "'$exp(1)' should be a SysFuncCall";
    EXPECT_EQ(call->getName(), "$exp");
    ASSERT_NE(call->getArguments(), nullptr);
    ASSERT_EQ(call->getArguments()->size(), 1u) << "20.8: '$exp' takes a single argument";

    const hldb::Constant *const arg = any_cast<hldb::Constant>(call->getArguments()->at(0));
    ASSERT_NE(arg, nullptr) << "'1' should be a Constant";
    EXPECT_EQ(arg->getConstType(), vpiUIntConst);
    EXPECT_EQ(arg->getSize(), 64);
    EXPECT_EQ(arg->getDecompile(), "1");

    const hldb::RefTypespec *const ref = arg->getTypespec();
    ASSERT_NE(ref, nullptr);
    EXPECT_NE(ref->getActual<hldb::IntTypespec>(), nullptr) << "'1' should have an IntTypespec";
  }
};

// --- module / initial process ------------------------------------------------

TEST_F(ExpFunctionTest, ModuleExists) { EXPECT_NE(getModule(), nullptr); }

TEST_F(ExpFunctionTest, ModuleHasOneInitialProcess) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getProcesses(), nullptr);
  EXPECT_EQ(mod->getProcesses()->size(), 1u);
  EXPECT_NE(getInitialProcess(), nullptr);
}

TEST_F(ExpFunctionTest, InitialBodyIsBeginWithOneStmt) {
  const hldb::Begin *const body = getInitialBody();
  ASSERT_NE(body, nullptr) << "'initial begin ... end' should wrap the body in a Begin";
  EXPECT_TRUE(body->getVariables() == nullptr || body->getVariables()->empty())
      << "the begin block declares no variables";
  ASSERT_NE(body->getStmts(), nullptr);
  EXPECT_EQ(body->getStmts()->size(), 1u) << "'$display(...);' is the only statement";
}

// --- $display(":assert: (%f > 2.718) and (%f < 2.719)", $exp(1), $exp(1)); ----

TEST_F(ExpFunctionTest, StmtIsDisplaySysTaskCall) {
  const hldb::SysTaskCall *const call = getDisplayCall();
  ASSERT_NE(call, nullptr) << "'$display(...)' should be a SysTaskCall";
  EXPECT_EQ(call->getName(), "$display");
}

TEST_F(ExpFunctionTest, DisplayCallHasFormatAndTwoExpArguments) {
  const hldb::SysTaskCall *const call = getDisplayCall();
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 3u);

  const hldb::Constant *const fmt = any_cast<hldb::Constant>(call->getArguments()->at(0));
  ASSERT_NE(fmt, nullptr) << "the format string should be a Constant";
  EXPECT_EQ(fmt->getConstType(), vpiStringConst);
  EXPECT_EQ(fmt->getSize(), 304) << "38 characters * 8 bits";
  EXPECT_EQ(fmt->getValue(), ":assert: (%f > 2.718) and (%f < 2.719)");
  const hldb::RefTypespec *const fmtRef = fmt->getTypespec();
  ASSERT_NE(fmtRef, nullptr);
  EXPECT_NE(fmtRef->getActual<hldb::StringTypespec>(), nullptr);

  EXPECT_NE(getExpCall(1), nullptr) << "the first '$exp(1)' should be a SysFuncCall";
  EXPECT_NE(getExpCall(2), nullptr) << "the second '$exp(1)' should be a SysFuncCall";
}

// --- $exp(1), $exp(1) ---------------------------------------------------------

TEST_F(ExpFunctionTest, FirstExpCallHasOneIntArgument) { expectExpCallOfOne(getExpCall(1)); }

TEST_F(ExpFunctionTest, SecondExpCallHasOneIntArgument) { expectExpCallOfOne(getExpCall(2)); }

TEST_F(ExpFunctionTest, ExpCallsAreDistinctObjects) {
  const hldb::SysFuncCall *const first = getExpCall(1);
  const hldb::SysFuncCall *const second = getExpCall(2);
  ASSERT_NE(first, nullptr);
  ASSERT_NE(second, nullptr);
  EXPECT_NE(first, second) << "each '$exp(1)' in the source should get its own SysFuncCall";
}

// --- compiler diagnostics -----------------------------------------------------

TEST_F(ExpFunctionTest, CompilesWithNoErrors) { EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND), nullptr); }

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
