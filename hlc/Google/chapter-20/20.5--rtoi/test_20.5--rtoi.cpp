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

// Tests for 20.5--rtoi.sv (tags: 20.5)
//   module top();
//   initial begin
//   	$display(":assert: (%d == 21)", $rtoi(21.37));
//   end
//   endmodule
//
// IEEE 1800-2023 Sec 20.5, "Conversion functions":
// "function integer $rtoi ( real_val );" -- "$rtoi converts real values to
// an integer type by truncating the real value (for example, 123.45 becomes
// 123)." It takes exactly one argument: the real value to convert.
//
// Checked:
//   - design has module "top" with exactly 1 process, and it is an Initial
//   - the Initial's body is a Begin (from the explicit "begin ... end")
//     wrapping exactly 1 statement and no variables
//   - that statement is a SysTaskCall named "$display" with exactly 2
//     NamedArgument arguments: the first's high conn is a Constant string
//     ":assert: (%d == 21)" (size 152) and the second's high conn is a
//     SysFuncCall named "$rtoi"
//   - the "$rtoi" SysFuncCall has exactly 1 NamedArgument argument whose
//     high conn is a Constant real "21.37" (size 64) with a typespec that
//     resolves to a RealTypespec
//   - compiler reports zero errors
//
// NOT CHECKED: runtime effects (that $rtoi(21.37) actually truncates to 21
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

class RtoiFunctionTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "20.5--rtoi.hlc"}); }
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

  static const hldb::SysFuncCall *getRtoiCall() {
    const hldb::SysTaskCall *const display = getDisplayCall();
    if (display == nullptr || display->getArguments() == nullptr || display->getArguments()->size() < 2u) {
      return nullptr;
    }
    const hldb::NamedArgument *const arg1 = display->getArguments()->at(1);
    if (arg1 == nullptr) {
      return nullptr;
    }
    return arg1->getHighConn<hldb::SysFuncCall>();
  }
};

// --- module / initial process ------------------------------------------------

TEST_F(RtoiFunctionTest, ModuleExists) { EXPECT_NE(getModule(), nullptr); }

TEST_F(RtoiFunctionTest, ModuleHasOneInitialProcess) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getProcesses(), nullptr);
  EXPECT_EQ(mod->getProcesses()->size(), 1u);
  EXPECT_NE(getInitialProcess(), nullptr);
}

TEST_F(RtoiFunctionTest, InitialBodyIsBeginWithOneStmt) {
  const hldb::Begin *const body = getInitialBody();
  ASSERT_NE(body, nullptr) << "'initial begin ... end' should wrap the body in a Begin";
  EXPECT_TRUE(body->getVariables() == nullptr || body->getVariables()->empty())
      << "the begin block declares no variables";
  ASSERT_NE(body->getStmts(), nullptr);
  EXPECT_EQ(body->getStmts()->size(), 1u) << "'$display(...);' is the only statement";
}

// --- $display(":assert: (%d == 21)", $rtoi(21.37)); ----------------------------

TEST_F(RtoiFunctionTest, StmtIsDisplaySysTaskCall) {
  const hldb::SysTaskCall *const call = getDisplayCall();
  ASSERT_NE(call, nullptr) << "'$display(...)' should be a SysTaskCall";
  EXPECT_EQ(call->getName(), "$display");
}

TEST_F(RtoiFunctionTest, DisplayCallHasFormatAndRtoiArgument) {
  const hldb::SysTaskCall *const call = getDisplayCall();
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 2u);

  const hldb::NamedArgument *const arg0 = call->getArguments()->at(0);
  ASSERT_NE(arg0, nullptr);
  const hldb::Constant *const fmt = arg0->getHighConn<hldb::Constant>();
  ASSERT_NE(fmt, nullptr) << "the format string should be a Constant";
  EXPECT_EQ(fmt->getConstType(), vpiStringConst);
  EXPECT_EQ(fmt->getSize(), 152) << "19 characters * 8 bits";
  EXPECT_EQ(fmt->getValue(), ":assert: (%d == 21)");
  const hldb::RefTypespec *const fmtRef = fmt->getTypespec();
  ASSERT_NE(fmtRef, nullptr);
  EXPECT_NE(fmtRef->getActual<hldb::StringTypespec>(), nullptr);

  EXPECT_NE(getRtoiCall(), nullptr) << "'$rtoi(21.37)' should be a SysFuncCall";
}

// --- $rtoi(21.37) -----------------------------------------------------------------

TEST_F(RtoiFunctionTest, RtoiCallIsNamedCorrectly) {
  const hldb::SysFuncCall *const call = getRtoiCall();
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getName(), "$rtoi");
}

TEST_F(RtoiFunctionTest, RtoiCallHasOneRealArgument) {
  const hldb::SysFuncCall *const call = getRtoiCall();
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 1u) << "20.5: '$rtoi' takes a single real_val argument";

  const hldb::NamedArgument *const arg0 = call->getArguments()->at(0);
  ASSERT_NE(arg0, nullptr);
  const hldb::Constant *const arg = arg0->getHighConn<hldb::Constant>();
  ASSERT_NE(arg, nullptr) << "'21.37' should be a Constant";
  EXPECT_EQ(arg->getConstType(), vpiRealConst);
  EXPECT_EQ(arg->getSize(), 64);
  EXPECT_EQ(arg->getDecompile(), "21.37");

  const hldb::RefTypespec *const ref = arg->getTypespec();
  ASSERT_NE(ref, nullptr);
  EXPECT_NE(ref->getActual<hldb::RealTypespec>(), nullptr) << "'21.37' should have a RealTypespec";
}

// --- compiler diagnostics -----------------------------------------------------

TEST_F(RtoiFunctionTest, CompilesWithNoErrors) { EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND), nullptr); }

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
