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

// Tests for 20.5--itor.sv (tags: 20.5)
//   module top();
//   initial begin
//   	$display(":assert: (%f == 20.0)", $itor(20));
//   end
//   endmodule
//
// IEEE 1800-2023 Sec 20.5, "Conversion functions":
// "function real $itor ( int_val );" -- "$itor converts integer values to
// real values (for example, 123 becomes 123.0)." It takes exactly one
// argument: the integer value to convert.
//
// Checked:
//   - design has module "top" with exactly 1 process, and it is an Initial
//   - the Initial's body is a Begin (from the explicit "begin ... end")
//     wrapping exactly 1 statement and no variables
//   - that statement is a SysTaskCall named "$display" with exactly 2
//     NamedArgument arguments: the first's high conn is a Constant string
//     ":assert: (%f == 20.0)" (size 168) and the second's high conn is a
//     SysFuncCall named "$itor"
//   - the "$itor" SysFuncCall has exactly 1 NamedArgument argument whose
//     high conn is a Constant unsigned int "20" (size 64) with a typespec
//     that resolves to a signed IntTypespec
//   - compiler reports zero errors
//
// NOT CHECKED: runtime effects (that $itor(20) actually returns 20.0 and
// that the ":assert:" comparison holds) cannot be observed -- HLC is a
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
#include <hldb/named_argument.h>
#include <hldb/ref_typespec.h>
#include <hldb/string_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/sys_func_call.h>
#include <hldb/sys_task_call.h>
#include <hldb/vpi_user.h>

namespace hlc {

class ItorFunctionTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "20.5--itor.hlc"}); }
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

  static const hldb::SysFuncCall *getItorCall() {
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
};

// --- module / initial process ------------------------------------------------

TEST_F(ItorFunctionTest, ModuleExists) { EXPECT_NE(getModule(), nullptr); }

TEST_F(ItorFunctionTest, ModuleHasOneInitialProcess) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getProcesses(), nullptr);
  EXPECT_EQ(mod->getProcesses()->size(), 1u);
  EXPECT_NE(getInitialProcess(), nullptr);
}

TEST_F(ItorFunctionTest, InitialBodyIsBeginWithOneStmt) {
  const hldb::Begin *const body = getInitialBody();
  ASSERT_NE(body, nullptr) << "'initial begin ... end' should wrap the body in a Begin";
  EXPECT_TRUE(body->getVariables() == nullptr || body->getVariables()->empty())
      << "the begin block declares no variables";
  ASSERT_NE(body->getStmts(), nullptr);
  EXPECT_EQ(body->getStmts()->size(), 1u) << "'$display(...);' is the only statement";
}

// --- $display(":assert: (%f == 20.0)", $itor(20)); -----------------------------

TEST_F(ItorFunctionTest, StmtIsDisplaySysTaskCall) {
  const hldb::SysTaskCall *const call = getDisplayCall();
  ASSERT_NE(call, nullptr) << "'$display(...)' should be a SysTaskCall";
  EXPECT_EQ(call->getName(), "$display");
}

TEST_F(ItorFunctionTest, DisplayCallHasFormatAndItorArgument) {
  const hldb::SysTaskCall *const call = getDisplayCall();
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 2u);

  const hldb::NamedArgument *const arg0 = any_cast<hldb::NamedArgument>(call->getArguments()->at(0));
  ASSERT_NE(arg0, nullptr);
  const hldb::Constant *const fmt = arg0->getHighConn<hldb::Constant>();
  ASSERT_NE(fmt, nullptr) << "the format string should be a Constant";
  EXPECT_EQ(fmt->getConstType(), vpiStringConst);
  EXPECT_EQ(fmt->getSize(), 168) << "21 characters * 8 bits";
  EXPECT_EQ(fmt->getValue(), ":assert: (%f == 20.0)");
  const hldb::RefTypespec *const fmtRef = fmt->getTypespec();
  ASSERT_NE(fmtRef, nullptr);
  EXPECT_NE(fmtRef->getActual<hldb::StringTypespec>(), nullptr);

  EXPECT_NE(getItorCall(), nullptr) << "'$itor(20)' should be a SysFuncCall";
}

// --- $itor(20) -------------------------------------------------------------------

TEST_F(ItorFunctionTest, ItorCallIsNamedCorrectly) {
  const hldb::SysFuncCall *const call = getItorCall();
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getName(), "$itor");
}

TEST_F(ItorFunctionTest, ItorCallHasOneIntegerArgument) {
  const hldb::SysFuncCall *const call = getItorCall();
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 1u) << "20.5: '$itor' takes a single int_val argument";

  const hldb::NamedArgument *const arg0 = any_cast<hldb::NamedArgument>(call->getArguments()->at(0));
  ASSERT_NE(arg0, nullptr);
  const hldb::Constant *const arg = arg0->getHighConn<hldb::Constant>();
  ASSERT_NE(arg, nullptr) << "'20' should be a Constant";
  EXPECT_EQ(arg->getConstType(), vpiUIntConst);
  EXPECT_EQ(arg->getSize(), 64);
  EXPECT_EQ(arg->getDecompile(), "20");
  EXPECT_EQ(arg->getValue(), "20");

  const hldb::RefTypespec *const ref = arg->getTypespec();
  ASSERT_NE(ref, nullptr);
  const hldb::IntTypespec *const ts = ref->getActual<hldb::IntTypespec>();
  ASSERT_NE(ts, nullptr) << "'20' should have an IntTypespec";
  EXPECT_TRUE(ts->getSigned());
}

// --- compiler diagnostics -----------------------------------------------------

TEST_F(ItorFunctionTest, CompilesWithNoErrors) { EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND), nullptr); }

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
