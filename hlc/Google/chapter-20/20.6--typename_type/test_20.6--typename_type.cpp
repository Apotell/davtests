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
// HLC records the data_type as the call's single argument: a NamedArgument
// whose high conn is a RefTypespec resolving to the LogicTypespec created
// for "logic".
//
// Checked:
//   - design has module "top" with exactly 1 process, and it is an Initial
//   - the Initial's body is a Begin (from the explicit "begin ... end")
//     wrapping exactly 1 statement and no variables, and owning exactly 1
//     typespec: the LogicTypespec created for the "logic" argument
//   - the statement is a SysTaskCall named "$display" with exactly 2
//     NamedArgument arguments: the first's high conn is a Constant string
//     ":assert: ('%s' == 'logic')" (size 208) and the second's high conn is
//     a SysFuncCall named "$typename"
//   - the "$typename" SysFuncCall has exactly 1 NamedArgument argument
//     whose high conn is a RefTypespec resolving to that Begin-owned
//     LogicTypespec, which is scalar (not a vector, no packed ranges) since
//     "logic" declares no dimensions
//   - compiler reports zero errors
//
// NOT CHECKED: runtime effects (that $typename(logic) actually returns
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
    const hldb::NamedArgument *const arg1 = any_cast<hldb::NamedArgument>(display->getArguments()->at(1));
    if (arg1 == nullptr) {
      return nullptr;
    }
    return arg1->getHighConn<hldb::SysFuncCall>();
  }

  static const hldb::RefTypespec *getTypenameArgument() {
    const hldb::SysFuncCall *const call = getTypenameCall();
    if (call == nullptr || call->getArguments() == nullptr || call->getArguments()->empty()) {
      return nullptr;
    }
    const hldb::NamedArgument *const arg0 = any_cast<hldb::NamedArgument>(call->getArguments()->at(0));
    if (arg0 == nullptr) {
      return nullptr;
    }
    return arg0->getHighConn<hldb::RefTypespec>();
  }

  static const hldb::LogicTypespec *getTypenameArgumentType() {
    const hldb::RefTypespec *const ref = getTypenameArgument();
    if (ref == nullptr) {
      return nullptr;
    }
    return ref->getActual<hldb::LogicTypespec>();
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

  EXPECT_NE(getTypenameCall(), nullptr) << "'$typename(logic)' should be a SysFuncCall";
}

// --- $typename(logic) -------------------------------------------------------------

TEST_F(TypenameTypeFunctionTest, TypenameCallIsNamedCorrectly) {
  const hldb::SysFuncCall *const call = getTypenameCall();
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getName(), "$typename");
}

TEST_F(TypenameTypeFunctionTest, TypenameCallHasOneTypespecArgument) {
  const hldb::SysFuncCall *const call = getTypenameCall();
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 1u) << "20.6.1: '$typename' takes a single expression or data_type";

  const hldb::NamedArgument *const arg0 = any_cast<hldb::NamedArgument>(call->getArguments()->at(0));
  ASSERT_NE(arg0, nullptr);
  EXPECT_NE(arg0->getHighConn<hldb::RefTypespec>(), nullptr) << "the 'logic' data_type should be a RefTypespec";
}

TEST_F(TypenameTypeFunctionTest, TypenameArgumentIsScalarLogic) {
  const hldb::RefTypespec *const ref = getTypenameArgument();
  ASSERT_NE(ref, nullptr) << "the 'logic' data_type argument should be a RefTypespec";
  ASSERT_NE(ref->getActual(), nullptr);
  EXPECT_EQ(ref->getActual()->getVpiType(), vpiLogicTypespec);

  const hldb::LogicTypespec *const ts = getTypenameArgumentType();
  ASSERT_NE(ts, nullptr);
  EXPECT_FALSE(ts->getVector()) << "'logic' declares no packed dimension";
  EXPECT_TRUE(ts->getRanges() == nullptr || ts->getRanges()->empty()) << "'logic' has no packed range";
}

// --- compiler diagnostics -----------------------------------------------------

TEST_F(TypenameTypeFunctionTest, CompilesWithNoErrors) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
