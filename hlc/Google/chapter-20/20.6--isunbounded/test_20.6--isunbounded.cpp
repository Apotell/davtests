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

// Tests for 20.6--isunbounded.sv (tags: 20.6)
//   module top();
//   parameter int i = $;
//   initial begin
//   	$display(":assert: (%d == 0)", $isunbounded(1));
//   	$display(":assert: (%d == 1)", $isunbounded(i));
//   end
//   endmodule
//
// IEEE 1800-2023 Sec 20.6.3, "Range system function":
// "$isunbounded ( constant_expression )" -- "$isunbounded returns true if
// the argument is $." It takes exactly one constant expression argument;
// here it is called once with the literal 1 and once with the parameter
// "i", whose value is the unbounded literal "$" (Sec 6.20.2).
//
// Checked:
//   - design has module "top" with exactly 1 Parameter "i" whose typespec
//     resolves to a signed IntTypespec
//   - module has exactly 1 ParamAssign: Lhs RefObj "i" resolving to that
//     Parameter, Rhs Constant vpiUnboundedConst "$"
//   - module has exactly 1 process, an Initial whose body is a Begin
//     wrapping exactly 2 statements and no variables
//   - both statements are SysTaskCall "$display" with exactly 2
//     NamedArgument arguments: the first's high conn is a Constant string
//     format (size 144) and the second's high conn is a SysFuncCall
//     "$isunbounded"
//   - the first "$isunbounded" has exactly 1 NamedArgument argument whose
//     high conn is a Constant unsigned int "1" (size 64, IntTypespec)
//   - the second "$isunbounded" has exactly 1 NamedArgument argument whose
//     high conn is a RefObj "i" resolving to the Parameter "i"
//   - compiler reports zero errors
//
// NOT CHECKED:
//   - the typespec of the "$" Constant: the standard gives "$" no type of
//     its own, so whatever typespec HLC attaches is an implementation
//     detail rather than something to assert.
//   - runtime effects (that $isunbounded returns 0 for 1 and 1 for i, and
//     that the ":assert:" comparisons hold) cannot be observed -- HLC is a
//     compiler/elaborator with no simulation capability, so no execution
//     ever happens for this test to check.

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
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/string_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/sys_func_call.h>
#include <hldb/sys_task_call.h>
#include <hldb/vpi_user.h>

namespace hlc {

class IsUnboundedFunctionTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "20.6--isunbounded.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getModule() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::Parameter *getParameterI() {
    const hldb::Module *const mod = getModule();
    if (mod == nullptr || mod->getParameters() == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::Parameter>("i", mod->getParameters());
  }

  static const hldb::ParamAssign *getParamAssign() {
    const hldb::Module *const mod = getModule();
    if (mod == nullptr || mod->getParamAssigns() == nullptr || mod->getParamAssigns()->empty()) {
      return nullptr;
    }
    return mod->getParamAssigns()->at(0);
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

  static const hldb::SysTaskCall *getDisplayCall(size_t index) {
    const hldb::Begin *const body = getInitialBody();
    if (body == nullptr || body->getStmts() == nullptr || body->getStmts()->size() <= index) {
      return nullptr;
    }
    return any_cast<hldb::SysTaskCall>(body->getStmts()->at(index));
  }

  static const hldb::SysFuncCall *getIsUnboundedCall(size_t index) {
    const hldb::SysTaskCall *const display = getDisplayCall(index);
    if (display == nullptr || display->getArguments() == nullptr || display->getArguments()->size() < 2u) {
      return nullptr;
    }
    const hldb::NamedArgument *const arg1 = display->getArguments()->at(1);
    if (arg1 == nullptr) {
      return nullptr;
    }
    return arg1->getHighConn<hldb::SysFuncCall>();
  }

  // Checks one '$display(<format>, $isunbounded(...))' statement: the format
  // Constant string and that the second argument is a "$isunbounded" call
  // with exactly 1 argument.
  static void checkDisplayCall(size_t index, std::string_view format) {
    const hldb::SysTaskCall *const call = getDisplayCall(index);
    ASSERT_NE(call, nullptr) << "statement " << index << " should be a SysTaskCall";
    EXPECT_EQ(call->getName(), "$display");
    ASSERT_NE(call->getArguments(), nullptr);
    ASSERT_EQ(call->getArguments()->size(), 2u);

    const hldb::NamedArgument *const arg0 = call->getArguments()->at(0);
    ASSERT_NE(arg0, nullptr);
    const hldb::Constant *const fmt = arg0->getHighConn<hldb::Constant>();
    ASSERT_NE(fmt, nullptr) << "the format string should be a Constant";
    EXPECT_EQ(fmt->getConstType(), vpiStringConst);
    EXPECT_EQ(fmt->getSize(), 144) << "18 characters * 8 bits";
    EXPECT_EQ(fmt->getValue(), format);
    const hldb::RefTypespec *const fmtRef = fmt->getTypespec();
    ASSERT_NE(fmtRef, nullptr);
    EXPECT_NE(fmtRef->getActual<hldb::StringTypespec>(), nullptr);

    const hldb::SysFuncCall *const isUnbounded = getIsUnboundedCall(index);
    ASSERT_NE(isUnbounded, nullptr) << "'$isunbounded(...)' should be a SysFuncCall";
    EXPECT_EQ(isUnbounded->getName(), "$isunbounded");
    ASSERT_NE(isUnbounded->getArguments(), nullptr);
    EXPECT_EQ(isUnbounded->getArguments()->size(), 1u) << "20.6.3: '$isunbounded' takes a single constant_expression";
  }
};

// --- module / parameter int i = $; --------------------------------------------

TEST_F(IsUnboundedFunctionTest, ModuleExists) { EXPECT_NE(getModule(), nullptr); }

TEST_F(IsUnboundedFunctionTest, ModuleHasSignedIntParameterI) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getParameters(), nullptr);
  EXPECT_EQ(mod->getParameters()->size(), 1u) << "'parameter int i' is the only parameter";

  const hldb::Parameter *const param = getParameterI();
  ASSERT_NE(param, nullptr) << "'i' should be a Parameter";
  const hldb::RefTypespec *const ref = param->getTypespec();
  ASSERT_NE(ref, nullptr);
  const hldb::IntTypespec *const ts = ref->getActual<hldb::IntTypespec>();
  ASSERT_NE(ts, nullptr) << "'parameter int i' should have an IntTypespec";
  EXPECT_TRUE(ts->getSigned()) << "'int' is a signed type";
}

TEST_F(IsUnboundedFunctionTest, ParamAssignBindsIToUnboundedLiteral) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getParamAssigns(), nullptr);
  EXPECT_EQ(mod->getParamAssigns()->size(), 1u);

  const hldb::ParamAssign *const assign = getParamAssign();
  ASSERT_NE(assign, nullptr);

  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr) << "'i' should be a RefObj";
  EXPECT_EQ(lhs->getName(), "i");
  EXPECT_EQ(lhs->getActual<hldb::Parameter>(), getParameterI());

  const hldb::Constant *const rhs = assign->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr) << "'$' should be a Constant";
  EXPECT_EQ(rhs->getConstType(), vpiUnboundedConst);
  EXPECT_EQ(rhs->getDecompile(), "$");
}

// --- initial process ------------------------------------------------------------

TEST_F(IsUnboundedFunctionTest, ModuleHasOneInitialProcess) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getProcesses(), nullptr);
  EXPECT_EQ(mod->getProcesses()->size(), 1u);
  EXPECT_NE(getInitialProcess(), nullptr);
}

TEST_F(IsUnboundedFunctionTest, InitialBodyIsBeginWithTwoStmts) {
  const hldb::Begin *const body = getInitialBody();
  ASSERT_NE(body, nullptr) << "'initial begin ... end' should wrap the body in a Begin";
  EXPECT_TRUE(body->getVariables() == nullptr || body->getVariables()->empty())
      << "the begin block declares no variables";
  ASSERT_NE(body->getStmts(), nullptr);
  EXPECT_EQ(body->getStmts()->size(), 2u) << "two '$display(...);' statements";
}

// --- $display(":assert: (%d == 0)", $isunbounded(1)); ---------------------------

TEST_F(IsUnboundedFunctionTest, FirstDisplayCallsIsUnbounded) { checkDisplayCall(0, ":assert: (%d == 0)"); }

TEST_F(IsUnboundedFunctionTest, FirstIsUnboundedArgumentIsLiteral1) {
  const hldb::SysFuncCall *const call = getIsUnboundedCall(0);
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 1u);

  const hldb::NamedArgument *const arg0 = call->getArguments()->at(0);
  ASSERT_NE(arg0, nullptr);
  const hldb::Constant *const arg = arg0->getHighConn<hldb::Constant>();
  ASSERT_NE(arg, nullptr) << "'1' should be a Constant";
  EXPECT_EQ(arg->getConstType(), vpiUIntConst);
  EXPECT_EQ(arg->getSize(), 64);
  EXPECT_EQ(arg->getDecompile(), "1");
  const hldb::RefTypespec *const ref = arg->getTypespec();
  ASSERT_NE(ref, nullptr);
  EXPECT_NE(ref->getActual<hldb::IntTypespec>(), nullptr);
}

// --- $display(":assert: (%d == 1)", $isunbounded(i)); ---------------------------

TEST_F(IsUnboundedFunctionTest, SecondDisplayCallsIsUnbounded) { checkDisplayCall(1, ":assert: (%d == 1)"); }

TEST_F(IsUnboundedFunctionTest, SecondIsUnboundedArgumentIsParameterI) {
  const hldb::SysFuncCall *const call = getIsUnboundedCall(1);
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 1u);

  const hldb::NamedArgument *const arg0 = call->getArguments()->at(0);
  ASSERT_NE(arg0, nullptr);
  const hldb::RefObj *const arg = arg0->getHighConn<hldb::RefObj>();
  ASSERT_NE(arg, nullptr) << "'i' should be a RefObj";
  EXPECT_EQ(arg->getName(), "i");
  EXPECT_EQ(arg->getActual<hldb::Parameter>(), getParameterI());
}

// --- compiler diagnostics -----------------------------------------------------

TEST_F(IsUnboundedFunctionTest, CompilesWithNoErrors) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
