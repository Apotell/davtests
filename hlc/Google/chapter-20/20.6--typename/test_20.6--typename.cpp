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

// Tests for 20.6--typename.sv (tags: 20.6)
//   module top();
//   initial begin
//   	logic val;
//   	$display(":assert: ('%s' == 'logic')", $typename(val));
//   end
//   endmodule
//
// IEEE 1800-2023 Sec 20.6.1, "Type name function":
// "typename_function ::= $typename ( expression ) | $typename ( data_type )"
// -- "The $typename system function returns a string that represents the
// resolved type of its argument." Here it is called with a single
// expression argument: the variable "val".
//
// Checked:
//   - design has module "top" with exactly 1 process, and it is an Initial
//   - the Initial's body is a Begin (from the explicit "begin ... end")
//     wrapping exactly 1 variable and 1 statement
//   - the variable is "val": LogicTypespec, scalar (not a vector, no
//     packed ranges) since "logic val" declares no dimensions
//   - the statement is a SysTaskCall named "$display" with exactly 2
//     NamedArgument arguments: the first's high conn is a Constant string
//     ":assert: ('%s' == 'logic')" (size 208) and the second's high conn is
//     a SysFuncCall named "$typename"
//   - the "$typename" SysFuncCall has exactly 1 NamedArgument argument
//     whose high conn is a RefObj "val" resolving to the Variable "val"
//   - compiler reports zero errors
//
// NOT CHECKED: runtime effects (that $typename(val) actually returns
// "logic" and that the ":assert:" comparison holds) cannot be observed --
// HLC is a compiler/elaborator with no simulation capability, so no
// execution ever happens for this test to check.

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
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/named_argument.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/string_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/sys_func_call.h>
#include <hldb/sys_task_call.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class TypenameFunctionTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "20.6--typename.hlc"}); }
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

  static const hldb::Variable *getValVariable() {
    const hldb::Begin *const body = getInitialBody();
    if (body == nullptr || body->getVariables() == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::Variable>("val", body->getVariables());
  }

  static const hldb::SysTaskCall *getDisplayCall() {
    const hldb::Begin *const body = getInitialBody();
    if (body == nullptr || body->getStmts() == nullptr || body->getStmts()->empty()) {
      return nullptr;
    }
    return any_cast<hldb::SysTaskCall>(body->getStmts()->at(0));
  }

  static const hldb::SysFuncCall *getTypenameCall() {
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

TEST_F(TypenameFunctionTest, ModuleExists) { EXPECT_NE(getModule(), nullptr); }

TEST_F(TypenameFunctionTest, ModuleHasOneInitialProcess) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getProcesses(), nullptr);
  EXPECT_EQ(mod->getProcesses()->size(), 1u);
  EXPECT_NE(getInitialProcess(), nullptr);
}

TEST_F(TypenameFunctionTest, InitialBodyIsBeginWithOneVariableAndOneStmt) {
  const hldb::Begin *const body = getInitialBody();
  ASSERT_NE(body, nullptr) << "'initial begin ... end' should wrap the body in a Begin";
  ASSERT_NE(body->getVariables(), nullptr);
  EXPECT_EQ(body->getVariables()->size(), 1u) << "'logic val;' is the only variable declaration";
  ASSERT_NE(body->getStmts(), nullptr);
  EXPECT_EQ(body->getStmts()->size(), 1u) << "'$display(...);' is the only statement";
}

// --- logic val; ----------------------------------------------------------------

TEST_F(TypenameFunctionTest, ValIsScalarLogicVariable) {
  const hldb::Variable *const val = getValVariable();
  ASSERT_NE(val, nullptr) << "'val' should be a Variable";
  EXPECT_EQ(val->getName(), "val");
  const hldb::RefTypespec *const ref = val->getTypespec<hldb::RefTypespec>();
  ASSERT_NE(ref, nullptr);
  const hldb::LogicTypespec *const ts = ref->getActual<hldb::LogicTypespec>();
  ASSERT_NE(ts, nullptr) << "'logic val' should have a LogicTypespec";
  EXPECT_FALSE(ts->getVector()) << "'logic val' declares no packed dimension";
  EXPECT_TRUE(ts->getRanges() == nullptr || ts->getRanges()->empty()) << "'logic val' has no packed range";
}

// --- $display(":assert: ('%s' == 'logic')", $typename(val)); --------------------

TEST_F(TypenameFunctionTest, StmtIsDisplaySysTaskCall) {
  const hldb::SysTaskCall *const call = getDisplayCall();
  ASSERT_NE(call, nullptr) << "'$display(...)' should be a SysTaskCall";
  EXPECT_EQ(call->getName(), "$display");
}

TEST_F(TypenameFunctionTest, DisplayCallHasFormatAndTypenameArgument) {
  const hldb::SysTaskCall *const call = getDisplayCall();
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 2u);

  const hldb::NamedArgument *const arg0 = any_cast<hldb::NamedArgument>(call->getArguments()->at(0));
  ASSERT_NE(arg0, nullptr);
  const hldb::Constant *const fmt = arg0->getHighConn<hldb::Constant>();
  ASSERT_NE(fmt, nullptr) << "the format string should be a Constant";
  EXPECT_EQ(fmt->getConstType(), vpiStringConst);
  EXPECT_EQ(fmt->getSize(), 208) << "26 characters * 8 bits";
  EXPECT_EQ(fmt->getValue(), ":assert: ('%s' == 'logic')");
  const hldb::RefTypespec *const fmtRef = fmt->getTypespec();
  ASSERT_NE(fmtRef, nullptr);
  EXPECT_NE(fmtRef->getActual<hldb::StringTypespec>(), nullptr);

  EXPECT_NE(getTypenameCall(), nullptr) << "'$typename(val)' should be a SysFuncCall";
}

// --- $typename(val) ---------------------------------------------------------------

TEST_F(TypenameFunctionTest, TypenameCallIsNamedCorrectly) {
  const hldb::SysFuncCall *const call = getTypenameCall();
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getName(), "$typename");
}

TEST_F(TypenameFunctionTest, TypenameCallHasValArgument) {
  const hldb::SysFuncCall *const call = getTypenameCall();
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 1u) << "20.6.1: '$typename' takes a single expression or data_type";

  const hldb::NamedArgument *const arg0 = any_cast<hldb::NamedArgument>(call->getArguments()->at(0));
  ASSERT_NE(arg0, nullptr);
  const hldb::RefObj *const arg = arg0->getHighConn<hldb::RefObj>();
  ASSERT_NE(arg, nullptr) << "'val' should be a RefObj";
  EXPECT_EQ(arg->getName(), "val");
  EXPECT_EQ(arg->getActual<hldb::Variable>(), getValVariable());
}

// --- compiler diagnostics -----------------------------------------------------

TEST_F(TypenameFunctionTest, CompilesWithNoErrors) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
