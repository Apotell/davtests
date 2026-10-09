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

// Tests for 20.8--atan2.sv (tags: 20.8)
//   module top();
//   initial begin
//   	$display("%f", $atan2(2.1, 3.7));
//   end
//   endmodule
//
// IEEE 1800-2023 Sec 20.8.2, "Real math functions", Table 20-4:
// "$atan2(y, x)" -- "Arc-tangent of y/x". It takes exactly two real
// arguments: y first, then x.
//
// Checked:
//   - design has module "top" with exactly 1 process, and it is an Initial
//   - the Initial's body is a Begin (from the explicit "begin ... end")
//     wrapping exactly 1 statement and no variables
//   - that statement is a SysTaskCall named "$display" with exactly 2
//     NamedArgument arguments: the first's high conn is a Constant string
//     "%f" (size 16) and the second's high conn is a SysFuncCall named
//     "$atan2"
//   - the "$atan2" SysFuncCall has exactly 2 NamedArgument arguments whose
//     high conns are, in order, a Constant real "2.1" (y) and a Constant
//     real "3.7" (x), each size 64 with a typespec that resolves to a
//     RealTypespec
//   - compiler reports zero errors
//
// NOT CHECKED: runtime effects (the numeric value of atan2(2.1, 3.7) that
// "%f" would print) cannot be observed -- HLC is a compiler/elaborator with
// no simulation capability, so no execution ever happens for this test to
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
#include <hldb/named_argument.h>
#include <hldb/real_typespec.h>
#include <hldb/ref_typespec.h>
#include <hldb/string_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/sys_func_call.h>
#include <hldb/sys_task_call.h>
#include <hldb/vpi_user.h>

namespace hlc {

class Atan2FunctionTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "20.8--atan2.hlc"}); }
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

  static const hldb::SysFuncCall *getAtan2Call() {
    const hldb::SysTaskCall *const display = getDisplayCall();
    if (display == nullptr || display->getArguments() == nullptr || display->getArguments()->size() < 2u) {
      return nullptr;
    }
    const hldb::NamedArgument *const arg1 = any_cast<hldb::NamedArgument>(display->getArguments()->at(1));
    if (arg1 == nullptr) {
      return nullptr;
    }
    return arg1->getHighConn<hldb::SysFuncCall>();
  }

  static void expectRealConstant(const hldb::NamedArgument *argument, std::string_view decompile) {
    ASSERT_NE(argument, nullptr);
    const hldb::Constant *const constant = argument->getHighConn<hldb::Constant>();
    ASSERT_NE(constant, nullptr) << "a real literal argument should be a Constant";
    EXPECT_EQ(constant->getConstType(), vpiRealConst);
    EXPECT_EQ(constant->getSize(), 64);
    EXPECT_EQ(constant->getDecompile(), decompile);

    const hldb::RefTypespec *const ref = constant->getTypespec();
    ASSERT_NE(ref, nullptr);
    EXPECT_NE(ref->getActual<hldb::RealTypespec>(), nullptr) << "a real literal should have a RealTypespec";
  }
};

// --- module / initial process ------------------------------------------------

TEST_F(Atan2FunctionTest, ModuleExists) { EXPECT_NE(getModule(), nullptr); }

TEST_F(Atan2FunctionTest, ModuleHasOneInitialProcess) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getProcesses(), nullptr);
  EXPECT_EQ(mod->getProcesses()->size(), 1u);
  EXPECT_NE(getInitialProcess(), nullptr);
}

TEST_F(Atan2FunctionTest, InitialBodyIsBeginWithOneStmt) {
  const hldb::Begin *const body = getInitialBody();
  ASSERT_NE(body, nullptr) << "'initial begin ... end' should wrap the body in a Begin";
  EXPECT_TRUE(body->getVariables() == nullptr || body->getVariables()->empty())
      << "the begin block declares no variables";
  ASSERT_NE(body->getStmts(), nullptr);
  EXPECT_EQ(body->getStmts()->size(), 1u) << "'$display(...);' is the only statement";
}

// --- $display("%f", $atan2(2.1, 3.7)); ---------------------------------------

TEST_F(Atan2FunctionTest, StmtIsDisplaySysTaskCall) {
  const hldb::SysTaskCall *const call = getDisplayCall();
  ASSERT_NE(call, nullptr) << "'$display(...)' should be a SysTaskCall";
  EXPECT_EQ(call->getName(), "$display");
}

TEST_F(Atan2FunctionTest, DisplayCallHasFormatAndAtan2Argument) {
  const hldb::SysTaskCall *const call = getDisplayCall();
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 2u);

  const hldb::NamedArgument *const arg0 = any_cast<hldb::NamedArgument>(call->getArguments()->at(0));
  ASSERT_NE(arg0, nullptr);
  const hldb::Constant *const fmt = arg0->getHighConn<hldb::Constant>();
  ASSERT_NE(fmt, nullptr) << "the format string should be a Constant";
  EXPECT_EQ(fmt->getConstType(), vpiStringConst);
  EXPECT_EQ(fmt->getSize(), 16) << "2 characters * 8 bits";
  EXPECT_EQ(fmt->getValue(), "%f");
  const hldb::RefTypespec *const fmtRef = fmt->getTypespec();
  ASSERT_NE(fmtRef, nullptr);
  EXPECT_NE(fmtRef->getActual<hldb::StringTypespec>(), nullptr);

  EXPECT_NE(getAtan2Call(), nullptr) << "'$atan2(2.1, 3.7)' should be a SysFuncCall";
}

// --- $atan2(2.1, 3.7) ------------------------------------------------------------

TEST_F(Atan2FunctionTest, Atan2CallIsNamedCorrectly) {
  const hldb::SysFuncCall *const call = getAtan2Call();
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getName(), "$atan2");
}

TEST_F(Atan2FunctionTest, Atan2CallHasTwoRealArguments) {
  const hldb::SysFuncCall *const call = getAtan2Call();
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 2u) << "20.8: '$atan2' takes y and x arguments";
}

TEST_F(Atan2FunctionTest, Atan2FirstArgumentIsRealY) {
  const hldb::SysFuncCall *const call = getAtan2Call();
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 2u);
  expectRealConstant(any_cast<hldb::NamedArgument>(call->getArguments()->at(0)), "2.1");
}

TEST_F(Atan2FunctionTest, Atan2SecondArgumentIsRealX) {
  const hldb::SysFuncCall *const call = getAtan2Call();
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 2u);
  expectRealConstant(any_cast<hldb::NamedArgument>(call->getArguments()->at(1)), "3.7");
}

// --- compiler diagnostics -----------------------------------------------------

TEST_F(Atan2FunctionTest, CompilesWithNoErrors) { EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND), nullptr); }

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
