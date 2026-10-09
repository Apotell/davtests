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

// Tests for 20.9--isunknown.sv (tags: 20.9)
//   module top();
//   initial begin
//   	parameter [3:0] val0 = 4'b000x;
//   	parameter [3:0] val1 = 4'b000z;
//   	parameter [3:0] val2 = 4'b00xz;
//   	parameter [3:0] val3 = 4'b0000;
//   	$display(":assert: (%d == 1)", $isunknown(val0));
//   	$display(":assert: (%d == 1)", $isunknown(val1));
//   	$display(":assert: (%d == 1)", $isunknown(val2));
//   	$display(":assert: (%d == 0)", $isunknown(val3));
//   end
//   endmodule
//
// IEEE 1800-2023 Sec 20.9, "Bit vector system functions":
// "$isunknown(expression)" returns 1'b1 if any bit of the expression is X
// or Z. It takes exactly one argument.
//
// Checked:
//   - design has module "top" with exactly 1 process, and it is an Initial
//   - the Initial's body is a Begin (from the explicit "begin ... end")
//     holding exactly 4 parameters, 4 parameter assignments, no variables
//     and 4 statements
//   - each parameter val0..val3 is typed by a LogicTypespec that is a vector
//     with exactly 1 range [3:0] (both bounds Constant unsigned int, size 64)
//   - each parameter assignment, in order, has a RefObj LHS bound to its
//     Parameter and a Constant binary RHS of size 4 ("4'b000x", "4'b000z",
//     "4'b00xz", "4'b0000") whose typespec resolves to a LogicTypespec
//   - each statement is a SysTaskCall named "$display" with exactly 2
//     arguments: a Constant string format (size 144) and a SysFuncCall named
//     "$isunknown" with exactly 1 argument, a RefObj bound to the Begin's
//     Parameter val0..val3 respectively
//   - compiler reports zero errors
//
// NOT CHECKED: runtime effects (the results 1, 1, 1 and 0 and that the
// ":assert:" comparisons hold) cannot be observed -- HLC is a
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
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/string_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/sys_func_call.h>
#include <hldb/sys_task_call.h>
#include <hldb/vpi_user.h>

namespace hlc {

class IsunknownFunctionTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "20.9--isunknown.hlc"}); }
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

  // "parameter [3:0] valN = ...;" -- declared inside the begin block.
  static const hldb::Parameter *getParam(std::string_view name) {
    const hldb::Begin *const body = getInitialBody();
    if (body == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::Parameter>(name, body->getParameters());
  }

  static const hldb::ParamAssign *getParamAssign(size_t index) {
    const hldb::Begin *const body = getInitialBody();
    if (body == nullptr || body->getParamAssigns() == nullptr || body->getParamAssigns()->size() <= index) {
      return nullptr;
    }
    return body->getParamAssigns()->at(index);
  }

  // stmtIndex is the position of the $display statement in the begin block.
  static const hldb::SysTaskCall *getDisplayCall(size_t stmtIndex) {
    const hldb::Begin *const body = getInitialBody();
    if (body == nullptr || body->getStmts() == nullptr || body->getStmts()->size() <= stmtIndex) {
      return nullptr;
    }
    return any_cast<hldb::SysTaskCall>(body->getStmts()->at(stmtIndex));
  }

  // The SysFuncCall passed as the second argument of the stmtIndex-th $display.
  static const hldb::SysFuncCall *getIsunknownCall(size_t stmtIndex) {
    const hldb::SysTaskCall *const display = getDisplayCall(stmtIndex);
    if (display == nullptr || display->getArguments() == nullptr || display->getArguments()->size() < 2u) {
      return nullptr;
    }
    return any_cast<hldb::SysFuncCall>(display->getArguments()->at(1));
  }

  // "[3:0]": a LogicTypespec vector with one range of unsigned int bounds.
  static void expectLogic3To0(const hldb::Parameter *param) {
    ASSERT_NE(param, nullptr);
    const hldb::RefTypespec *const ref = param->getTypespec();
    ASSERT_NE(ref, nullptr);
    const hldb::LogicTypespec *const logic = ref->getActual<hldb::LogicTypespec>();
    ASSERT_NE(logic, nullptr) << param->getName() << " should have a LogicTypespec";
    EXPECT_TRUE(logic->getVector());
    ASSERT_NE(logic->getRanges(), nullptr);
    ASSERT_EQ(logic->getRanges()->size(), 1u) << "'[3:0]' is a single packed range";

    const hldb::Range *const range = logic->getRanges()->at(0);
    ASSERT_NE(range, nullptr);
    const hldb::Constant *const left = range->getLeftExpr<hldb::Constant>();
    ASSERT_NE(left, nullptr);
    EXPECT_EQ(left->getConstType(), vpiUIntConst);
    EXPECT_EQ(left->getSize(), 64);
    EXPECT_EQ(left->getDecompile(), "3");
    const hldb::Constant *const right = range->getRightExpr<hldb::Constant>();
    ASSERT_NE(right, nullptr);
    EXPECT_EQ(right->getConstType(), vpiUIntConst);
    EXPECT_EQ(right->getSize(), 64);
    EXPECT_EQ(right->getDecompile(), "0");
  }

  // "valN = 4'b...": the LHS binds to the Parameter, the RHS is a 4-bit
  // binary Constant.
  static void expectParamAssign(size_t index, std::string_view name, std::string_view decompile) {
    const hldb::ParamAssign *const assign = getParamAssign(index);
    ASSERT_NE(assign, nullptr) << "parameter assignment " << index << " should exist";

    const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
    ASSERT_NE(lhs, nullptr) << "the LHS of '" << name << " = ...' should be a RefObj";
    EXPECT_EQ(lhs->getName(), name);
    ASSERT_NE(getParam(name), nullptr);
    EXPECT_EQ(lhs->getActual<hldb::Parameter>(), getParam(name));

    const hldb::Constant *const rhs = assign->getRhs<hldb::Constant>();
    ASSERT_NE(rhs, nullptr) << "the RHS of '" << name << " = ...' should be a Constant";
    EXPECT_EQ(rhs->getConstType(), vpiBinaryConst);
    EXPECT_EQ(rhs->getSize(), 4);
    EXPECT_EQ(rhs->getDecompile(), decompile);
    const hldb::RefTypespec *const ref = rhs->getTypespec();
    ASSERT_NE(ref, nullptr);
    EXPECT_NE(ref->getActual<hldb::LogicTypespec>(), nullptr);
  }

  // "$display(format, $isunknown(valN));"
  static void expectDisplayOfIsunknown(size_t stmtIndex, std::string_view format, std::string_view name) {
    const hldb::SysTaskCall *const display = getDisplayCall(stmtIndex);
    ASSERT_NE(display, nullptr) << "statement " << stmtIndex << " should be a $display SysTaskCall";
    EXPECT_EQ(display->getName(), "$display");
    ASSERT_NE(display->getArguments(), nullptr);
    ASSERT_EQ(display->getArguments()->size(), 2u);

    const hldb::Constant *const fmt = any_cast<hldb::Constant>(display->getArguments()->at(0));
    ASSERT_NE(fmt, nullptr) << "the format string should be a Constant";
    EXPECT_EQ(fmt->getConstType(), vpiStringConst);
    EXPECT_EQ(fmt->getSize(), 144) << "18 characters * 8 bits";
    EXPECT_EQ(fmt->getValue(), format);
    const hldb::RefTypespec *const fmtRef = fmt->getTypespec();
    ASSERT_NE(fmtRef, nullptr);
    EXPECT_NE(fmtRef->getActual<hldb::StringTypespec>(), nullptr);

    const hldb::SysFuncCall *const call = getIsunknownCall(stmtIndex);
    ASSERT_NE(call, nullptr) << "'$isunknown(" << name << ")' should be a SysFuncCall";
    EXPECT_EQ(call->getName(), "$isunknown");
    ASSERT_NE(call->getArguments(), nullptr);
    ASSERT_EQ(call->getArguments()->size(), 1u) << "20.9: '$isunknown' takes a single argument";

    const hldb::RefObj *const arg = any_cast<hldb::RefObj>(call->getArguments()->at(0));
    ASSERT_NE(arg, nullptr) << "'" << name << "' should be a RefObj";
    EXPECT_EQ(arg->getName(), name);
    ASSERT_NE(getParam(name), nullptr);
    EXPECT_EQ(arg->getActual<hldb::Parameter>(), getParam(name))
        << "'" << name << "' should bind to the Begin's Parameter";
  }
};

// --- module / initial process ------------------------------------------------

TEST_F(IsunknownFunctionTest, ModuleExists) { EXPECT_NE(getModule(), nullptr); }

TEST_F(IsunknownFunctionTest, ModuleHasOneInitialProcess) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getProcesses(), nullptr);
  EXPECT_EQ(mod->getProcesses()->size(), 1u);
  EXPECT_NE(getInitialProcess(), nullptr);
}

TEST_F(IsunknownFunctionTest, InitialBodyIsBeginWithFourParametersAndFourStmts) {
  const hldb::Begin *const body = getInitialBody();
  ASSERT_NE(body, nullptr) << "'initial begin ... end' should wrap the body in a Begin";
  ASSERT_NE(body->getParameters(), nullptr);
  EXPECT_EQ(body->getParameters()->size(), 4u) << "val0..val3 are declared in the begin block";
  ASSERT_NE(body->getParamAssigns(), nullptr);
  EXPECT_EQ(body->getParamAssigns()->size(), 4u) << "each parameter has one assignment";
  EXPECT_TRUE(body->getVariables() == nullptr || body->getVariables()->empty())
      << "the begin block declares parameters, not variables";
  ASSERT_NE(body->getStmts(), nullptr);
  EXPECT_EQ(body->getStmts()->size(), 4u) << "the begin block holds four '$display(...);' statements";
}

// --- parameter [3:0] val0..val3 = ...; ---------------------------------------

TEST_F(IsunknownFunctionTest, ParametersAreLogicVectors3To0) {
  expectLogic3To0(getParam("val0"));
  expectLogic3To0(getParam("val1"));
  expectLogic3To0(getParam("val2"));
  expectLogic3To0(getParam("val3"));
}

TEST_F(IsunknownFunctionTest, ParameterAssignmentsAreFourBitBinaryConstants) {
  expectParamAssign(0, "val0", "4'b000x");
  expectParamAssign(1, "val1", "4'b000z");
  expectParamAssign(2, "val2", "4'b00xz");
  expectParamAssign(3, "val3", "4'b0000");
}

// --- $display(":assert: (%d == N)", $isunknown(valN)); x 4 -------------------

TEST_F(IsunknownFunctionTest, DisplayIsunknownOfVal0) { expectDisplayOfIsunknown(0, ":assert: (%d == 1)", "val0"); }

TEST_F(IsunknownFunctionTest, DisplayIsunknownOfVal1) { expectDisplayOfIsunknown(1, ":assert: (%d == 1)", "val1"); }

TEST_F(IsunknownFunctionTest, DisplayIsunknownOfVal2) { expectDisplayOfIsunknown(2, ":assert: (%d == 1)", "val2"); }

TEST_F(IsunknownFunctionTest, DisplayIsunknownOfVal3) { expectDisplayOfIsunknown(3, ":assert: (%d == 0)", "val3"); }

// --- compiler diagnostics -----------------------------------------------------

TEST_F(IsunknownFunctionTest, CompilesWithNoErrors) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
