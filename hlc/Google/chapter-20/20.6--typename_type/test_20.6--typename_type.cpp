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

// Tests for 20.6--typename_type.sv (tags: 20.6)
//   module top();
//   initial begin
//   	$display(":assert: ('%s' == 'logic')", $typename(logic));
//   end
//   endmodule
//
// IEEE 1800-2023 Sec 20.6.1, "Type name function":
// "typename_function ::= $typename ( expression ) | $typename ( data_type )"
// -- "The $typename system function returns a string that represents the
// resolved type of its argument." Here it is called with the second form:
// a data_type argument, the built-in type "logic".
//
// The data_type is not an expression, but it still occupies the call's
// argument position. HLC records it as the call's single argument: a
// NamedArgument whose vpiHighConn is a RefTypespec (RefTypespec is a
// simple_expr in the HLDB model), with vpiActual pointing to a LogicTypespec.
// The LogicTypespec object itself is owned by the enclosing Begin's
// vpiTypespec list; the argument only references it. The SysFuncCall has no
// typespec of its own for "logic" (same shape as "$typename(logic)" and
// "$bits(real)" in the DatQuery01 test).
//
// Checked:
//   - design has module "top" with exactly 1 process, and it is an Initial
//   - the Initial's body is a Begin (from the explicit "begin ... end")
//     wrapping exactly 1 statement and no variables, and owning exactly 1
//     typespec: the LogicTypespec created for the "logic" argument
//   - the statement is a SysTaskCall named "$display" with exactly 2
//     arguments: a Constant string ":assert: ('%s' == 'logic')" (size 208)
//     and a SysFuncCall named "$typename"
//   - the "$typename" SysFuncCall has exactly 1 argument, a RefTypespec
//     whose actual is that Begin-owned LogicTypespec, which is scalar (not
//     a vector, no packed ranges) since "logic" declares no dimensions
//   - compiler reports zero errors
//
// NOT CHECKED: runtime effects (that $typename(logic) actually returns
// "logic" and that the ":assert:" comparison holds) cannot be observed --
// HLC is a compiler/elaborator with no simulation capability, so no
// execution ever happens for this test to check.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
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
#include <hldb/ref_typespec.h>
#include <hldb/string_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/sys_func_call.h>
#include <hldb/sys_task_call.h>
#include <hldb/vpi_user.h>

namespace hlc {

class TypenameTypeFunctionTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "20.6--typename_type.hlc"}); }
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

  static const hldb::SysFuncCall *getTypenameCall() {
    const hldb::SysTaskCall *const display = getDisplayCall();
    if (display == nullptr || display->getArguments() == nullptr || display->getArguments()->size() < 2u) {
      return nullptr;
    }
    const hldb::NamedArgument *const arg1 = display->getArguments()->back();
    return arg1->getHighConn<hldb::SysFuncCall>();
  }

  static const hldb::LogicTypespec *getTypenameArgumentType() {
    const hldb::SysFuncCall *const call = getTypenameCall();
    if (call == nullptr || call->getArguments() == nullptr || call->getArguments()->empty()) {
      return nullptr;
    }
    const hldb::NamedArgument *const arg = call->getArguments()->front();
    const hldb::RefTypespec *const rt = arg->getHighConn<hldb::RefTypespec>();
    return (rt == nullptr) ? nullptr : rt->getActual<hldb::LogicTypespec>();
  }
};

// --- module / initial process ------------------------------------------------

TEST_F(TypenameTypeFunctionTest, ModuleExists) { EXPECT_NE(getModule(), nullptr); }

TEST_F(TypenameTypeFunctionTest, ModuleHasOneInitialProcess) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getProcesses(), nullptr);
  EXPECT_EQ(mod->getProcesses()->size(), 1u);
  EXPECT_NE(getInitialProcess(), nullptr);
}

TEST_F(TypenameTypeFunctionTest, InitialBodyIsBeginWithOneStmt) {
  const hldb::Begin *const body = getInitialBody();
  ASSERT_NE(body, nullptr) << "'initial begin ... end' should wrap the body in a Begin";
  EXPECT_TRUE(body->getVariables() == nullptr || body->getVariables()->empty())
      << "the begin block declares no variables";
  ASSERT_NE(body->getStmts(), nullptr);
  EXPECT_EQ(body->getStmts()->size(), 1u) << "'$display(...);' is the only statement";
}

TEST_F(TypenameTypeFunctionTest, BeginOwnsTheLogicTypespec) {
  const hldb::Begin *const body = getInitialBody();
  ASSERT_NE(body, nullptr);
  ASSERT_NE(body->getTypespecs(), nullptr);
  ASSERT_EQ(body->getTypespecs()->size(), 1u) << "the 'logic' argument is the only type used in the block";
  EXPECT_EQ(body->getTypespecs()->at(0), getTypenameArgumentType());
}

// --- $display(":assert: ('%s' == 'logic')", $typename(logic)); ------------------

TEST_F(TypenameTypeFunctionTest, StmtIsDisplaySysTaskCall) {
  const hldb::SysTaskCall *const call = getDisplayCall();
  ASSERT_NE(call, nullptr) << "'$display(...)' should be a SysTaskCall";
  EXPECT_EQ(call->getName(), "$display");
}

TEST_F(TypenameTypeFunctionTest, DisplayCallHasFormatAndTypenameArgument) {
  const hldb::SysTaskCall *const call = getDisplayCall();
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 2u);

  const hldb::NamedArgument *const arg0 = call->getArguments()->front();
  const hldb::Constant *const fmt = arg0->getHighConn<hldb::Constant>();
  ASSERT_NE(fmt, nullptr) << "the format string should be a Constant";
  EXPECT_EQ(fmt->getConstType(), vpiStringConst);
  EXPECT_EQ(fmt->getSize(), 208) << "26 characters * 8 bits";
  EXPECT_EQ(fmt->getValue(), ":assert: ('%s' == 'logic')");
  const hldb::RefTypespec *const fmtRef = fmt->getTypespec();
  ASSERT_NE(fmtRef, nullptr);
  EXPECT_NE(fmtRef->getActual<hldb::StringTypespec>(), nullptr);

  EXPECT_NE(getTypenameCall(), nullptr) << "'$typename(logic)' should be a SysFuncCall";
}

// --- $typename(logic) -------------------------------------------------------------

TEST_F(TypenameTypeFunctionTest, TypenameCallIsNamedCorrectly) {
  const hldb::SysFuncCall *const call = getTypenameCall();
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getName(), "$typename");
}

TEST_F(TypenameTypeFunctionTest, TypenameCallHasOneArgument) {
  const hldb::SysFuncCall *const call = getTypenameCall();
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr);
  EXPECT_EQ(call->getArguments()->size(), 1u)
      << "20.6.1: '$typename ( data_type )' -- the 'logic' data_type is the call's single argument, "
         "recorded as a RefTypespec";
}

TEST_F(TypenameTypeFunctionTest, TypenameCallTypespecIsScalarLogic) {
  const hldb::SysFuncCall *const call = getTypenameCall();
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr);
  EXPECT_EQ(call->getArguments()->size(), 1u)
      << "the 'logic' data_type argument should be recorded as the call's argument";

  const hldb::LogicTypespec *const ts = getTypenameArgumentType();
  ASSERT_NE(ts, nullptr);
  EXPECT_FALSE(ts->getVector()) << "'logic' declares no packed dimension";
  EXPECT_TRUE(ts->getRanges() == nullptr || ts->getRanges()->empty()) << "'logic' has no packed range";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
