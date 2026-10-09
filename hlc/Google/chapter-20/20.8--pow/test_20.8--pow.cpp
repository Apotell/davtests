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

// Tests for 20.8--pow.sv (tags: 20.8)
//   module top();
//   initial begin
//   	$display(":assert: (%f == 5.0625)", $pow(2.25, 2));
//   end
//   endmodule
//
// IEEE 1800-2023 Sec 20.8.2, "Real math functions", Table 20-4:
// "$pow(x, y)" -- "x**y". It takes exactly two arguments: the base x first,
// then the exponent y.
//
// Checked:
//   - design has module "top" with exactly 1 process, and it is an Initial
//   - the Initial's body is a Begin (from the explicit "begin ... end")
//     wrapping exactly 1 statement and no variables
//   - that statement is a SysTaskCall named "$display" with exactly 2
//     arguments: a Constant string ":assert: (%f == 5.0625)" (size 184) and
//     a SysFuncCall named "$pow"
//   - the "$pow" SysFuncCall has exactly 2 arguments, in order:
//       - x: a Constant real "2.25" (size 64) whose typespec resolves to a
//         RealTypespec
//       - y: a Constant unsigned int "2" (size 64) whose typespec resolves
//         to an IntTypespec
//   - compiler reports zero errors
//
// NOT CHECKED: runtime effects (that $pow(2.25, 2) actually yields 5.0625
// and that the ":assert:" comparison holds) cannot be observed -- HLC is a
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
#include <hldb/real_typespec.h>
#include <hldb/ref_typespec.h>
#include <hldb/string_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/sys_func_call.h>
#include <hldb/sys_task_call.h>
#include <hldb/vpi_user.h>

namespace hlc {

class PowFunctionTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "20.8--pow.hlc"}); }
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

  static const hldb::SysFuncCall *getPowCall() {
    const hldb::SysTaskCall *const display = getDisplayCall();
    if (display == nullptr || display->getArguments() == nullptr || display->getArguments()->size() < 2u) {
      return nullptr;
    }
    return any_cast<hldb::SysFuncCall>(display->getArguments()->at(1));
  }
};

// --- module / initial process ------------------------------------------------

TEST_F(PowFunctionTest, ModuleExists) { EXPECT_NE(getModule(), nullptr); }

TEST_F(PowFunctionTest, ModuleHasOneInitialProcess) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getProcesses(), nullptr);
  EXPECT_EQ(mod->getProcesses()->size(), 1u);
  EXPECT_NE(getInitialProcess(), nullptr);
}

TEST_F(PowFunctionTest, InitialBodyIsBeginWithOneStmt) {
  const hldb::Begin *const body = getInitialBody();
  ASSERT_NE(body, nullptr) << "'initial begin ... end' should wrap the body in a Begin";
  EXPECT_TRUE(body->getVariables() == nullptr || body->getVariables()->empty())
      << "the begin block declares no variables";
  ASSERT_NE(body->getStmts(), nullptr);
  EXPECT_EQ(body->getStmts()->size(), 1u) << "'$display(...);' is the only statement";
}

// --- $display(":assert: (%f == 5.0625)", $pow(2.25, 2)); ---------------------

TEST_F(PowFunctionTest, StmtIsDisplaySysTaskCall) {
  const hldb::SysTaskCall *const call = getDisplayCall();
  ASSERT_NE(call, nullptr) << "'$display(...)' should be a SysTaskCall";
  EXPECT_EQ(call->getName(), "$display");
}

TEST_F(PowFunctionTest, DisplayCallHasFormatAndPowArgument) {
  const hldb::SysTaskCall *const call = getDisplayCall();
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 2u);

  const hldb::Constant *const fmt = any_cast<hldb::Constant>(call->getArguments()->at(0));
  ASSERT_NE(fmt, nullptr) << "the format string should be a Constant";
  EXPECT_EQ(fmt->getConstType(), vpiStringConst);
  EXPECT_EQ(fmt->getSize(), 184) << "23 characters * 8 bits";
  EXPECT_EQ(fmt->getValue(), ":assert: (%f == 5.0625)");
  const hldb::RefTypespec *const fmtRef = fmt->getTypespec();
  ASSERT_NE(fmtRef, nullptr);
  EXPECT_NE(fmtRef->getActual<hldb::StringTypespec>(), nullptr);

  EXPECT_NE(getPowCall(), nullptr) << "'$pow(2.25, 2)' should be a SysFuncCall";
}

// --- $pow(2.25, 2) ------------------------------------------------------------

TEST_F(PowFunctionTest, PowCallIsNamedCorrectly) {
  const hldb::SysFuncCall *const call = getPowCall();
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getName(), "$pow");
}

TEST_F(PowFunctionTest, PowCallHasTwoArguments) {
  const hldb::SysFuncCall *const call = getPowCall();
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 2u) << "20.8: '$pow' takes x and y arguments";
}

TEST_F(PowFunctionTest, PowFirstArgumentIsRealX) {
  const hldb::SysFuncCall *const call = getPowCall();
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 2u);

  const hldb::Constant *const x = any_cast<hldb::Constant>(call->getArguments()->at(0));
  ASSERT_NE(x, nullptr) << "'2.25' should be a Constant";
  EXPECT_EQ(x->getConstType(), vpiRealConst);
  EXPECT_EQ(x->getSize(), 64);
  EXPECT_EQ(x->getDecompile(), "2.25");

  const hldb::RefTypespec *const ref = x->getTypespec();
  ASSERT_NE(ref, nullptr);
  EXPECT_NE(ref->getActual<hldb::RealTypespec>(), nullptr) << "'2.25' should have a RealTypespec";
}

TEST_F(PowFunctionTest, PowSecondArgumentIsIntY) {
  const hldb::SysFuncCall *const call = getPowCall();
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 2u);

  const hldb::Constant *const y = any_cast<hldb::Constant>(call->getArguments()->at(1));
  ASSERT_NE(y, nullptr) << "'2' should be a Constant";
  EXPECT_EQ(y->getConstType(), vpiUIntConst);
  EXPECT_EQ(y->getSize(), 64);
  EXPECT_EQ(y->getDecompile(), "2");

  const hldb::RefTypespec *const ref = y->getTypespec();
  ASSERT_NE(ref, nullptr);
  EXPECT_NE(ref->getActual<hldb::IntTypespec>(), nullptr) << "'2' should have an IntTypespec";
}

// --- compiler diagnostics -----------------------------------------------------

TEST_F(PowFunctionTest, CompilesWithNoErrors) { EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND), nullptr); }

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
