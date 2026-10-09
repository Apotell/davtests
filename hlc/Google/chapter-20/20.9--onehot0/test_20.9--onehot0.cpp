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

// Tests for 20.9--onehot0.sv (tags: 20.9)
//   module top();
//   initial begin
//   	logic [31:0] val0 = 32'h00010000;
//   	logic [31:0] val1 = 32'h00030000;
//   	logic [31:0] val2 = 32'h00000000;
//   	$display(":assert: (%d == 1)", $onehot0(val0));
//   	$display(":assert: (%d == 0)", $onehot0(val1));
//   	$display(":assert: (%d == 1)", $onehot0(val2));
//   end
//   endmodule
//
// IEEE 1800-2023 Sec 20.9, "Bit vector system functions":
// "$onehot0(expression)" returns 1'b1 if at most one bit of the expression
// is high. It takes exactly one argument.
//
// Checked:
//   - design has module "top" with exactly 1 process, and it is an Initial
//   - the Initial's body is a Begin (from the explicit "begin ... end")
//     declaring exactly 3 variables and holding exactly 3 statements
//   - each variable val0..val2 is typed by a LogicTypespec that is a vector
//     with exactly 1 range [31:0] (both bounds Constant unsigned int, size
//     64), and is initialized by a Constant hex of size 32 ("32'h00010000",
//     "32'h00030000", "32'h00000000") whose typespec resolves to an
//     IntTypespec
//   - each statement is a SysTaskCall named "$display" with exactly 2
//     arguments: a Constant string format (size 144) and a SysFuncCall named
//     "$onehot0" with exactly 1 argument, a RefObj bound to the Begin's
//     Variable val0..val2 respectively
//   - compiler reports zero errors
//
// NOT CHECKED: runtime effects (the results 1, 0 and 1 and that the
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
#include <hldb/int_typespec.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/string_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/sys_func_call.h>
#include <hldb/sys_task_call.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class Onehot0FunctionTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "20.9--onehot0.hlc"}); }
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

  // "logic [31:0] valN = ...;" -- declared inside the begin block.
  static const hldb::Variable *getVar(std::string_view name) {
    const hldb::Begin *const body = getInitialBody();
    if (body == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::Variable>(name, body->getVariables());
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
  static const hldb::SysFuncCall *getOnehot0Call(size_t stmtIndex) {
    const hldb::SysTaskCall *const display = getDisplayCall(stmtIndex);
    if (display == nullptr || display->getArguments() == nullptr || display->getArguments()->size() < 2u) {
      return nullptr;
    }
    return any_cast<hldb::SysFuncCall>(display->getArguments()->at(1));
  }

  // "logic [31:0] name = decompile;"
  static void expectLogic31To0WithHexInit(std::string_view name, std::string_view decompile) {
    const hldb::Variable *const var = getVar(name);
    ASSERT_NE(var, nullptr) << "'" << name << "' should be a Variable of the begin block";

    const hldb::RefTypespec *const ref = var->getTypespec();
    ASSERT_NE(ref, nullptr);
    const hldb::LogicTypespec *const logic = ref->getActual<hldb::LogicTypespec>();
    ASSERT_NE(logic, nullptr) << "'logic [31:0]' should have a LogicTypespec";
    EXPECT_TRUE(logic->getVector());
    ASSERT_NE(logic->getRanges(), nullptr);
    ASSERT_EQ(logic->getRanges()->size(), 1u) << "'[31:0]' is a single packed range";

    const hldb::Range *const range = logic->getRanges()->at(0);
    ASSERT_NE(range, nullptr);
    const hldb::Constant *const left = range->getLeftExpr<hldb::Constant>();
    ASSERT_NE(left, nullptr);
    EXPECT_EQ(left->getConstType(), vpiUIntConst);
    EXPECT_EQ(left->getSize(), 64);
    EXPECT_EQ(left->getDecompile(), "31");
    const hldb::Constant *const right = range->getRightExpr<hldb::Constant>();
    ASSERT_NE(right, nullptr);
    EXPECT_EQ(right->getConstType(), vpiUIntConst);
    EXPECT_EQ(right->getSize(), 64);
    EXPECT_EQ(right->getDecompile(), "0");

    const hldb::Constant *const init = var->getValue<hldb::Constant>();
    ASSERT_NE(init, nullptr) << "'" << name << "' should have a Constant initializer";
    EXPECT_EQ(init->getConstType(), vpiHexConst);
    EXPECT_EQ(init->getSize(), 32);
    EXPECT_EQ(init->getDecompile(), decompile);
    const hldb::RefTypespec *const initRef = init->getTypespec();
    ASSERT_NE(initRef, nullptr);
    EXPECT_NE(initRef->getActual<hldb::IntTypespec>(), nullptr);
  }

  // "$display(format, $onehot0(name));"
  static void expectDisplayOfOnehot0(size_t stmtIndex, std::string_view format, std::string_view name) {
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

    const hldb::SysFuncCall *const call = getOnehot0Call(stmtIndex);
    ASSERT_NE(call, nullptr) << "'$onehot0(" << name << ")' should be a SysFuncCall";
    EXPECT_EQ(call->getName(), "$onehot0");
    ASSERT_NE(call->getArguments(), nullptr);
    ASSERT_EQ(call->getArguments()->size(), 1u) << "20.9: '$onehot0' takes a single argument";

    const hldb::RefObj *const arg = any_cast<hldb::RefObj>(call->getArguments()->at(0));
    ASSERT_NE(arg, nullptr) << "'" << name << "' should be a RefObj";
    EXPECT_EQ(arg->getName(), name);
    ASSERT_NE(getVar(name), nullptr);
    EXPECT_EQ(arg->getActual<hldb::Variable>(), getVar(name)) << "'" << name << "' should bind to the Begin's Variable";
  }
};

// --- module / initial process ------------------------------------------------

TEST_F(Onehot0FunctionTest, ModuleExists) { EXPECT_NE(getModule(), nullptr); }

TEST_F(Onehot0FunctionTest, ModuleHasOneInitialProcess) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getProcesses(), nullptr);
  EXPECT_EQ(mod->getProcesses()->size(), 1u);
  EXPECT_NE(getInitialProcess(), nullptr);
}

TEST_F(Onehot0FunctionTest, InitialBodyIsBeginWithThreeVariablesAndThreeStmts) {
  const hldb::Begin *const body = getInitialBody();
  ASSERT_NE(body, nullptr) << "'initial begin ... end' should wrap the body in a Begin";
  ASSERT_NE(body->getVariables(), nullptr);
  EXPECT_EQ(body->getVariables()->size(), 3u) << "val0..val2 are declared in the begin block";
  ASSERT_NE(body->getStmts(), nullptr);
  EXPECT_EQ(body->getStmts()->size(), 3u) << "the begin block holds three '$display(...);' statements";
}

// --- logic [31:0] val0..val2 = 32'h...; --------------------------------------

TEST_F(Onehot0FunctionTest, Val0IsLogicVectorWithHexInit) { expectLogic31To0WithHexInit("val0", "32'h00010000"); }

TEST_F(Onehot0FunctionTest, Val1IsLogicVectorWithHexInit) { expectLogic31To0WithHexInit("val1", "32'h00030000"); }

TEST_F(Onehot0FunctionTest, Val2IsLogicVectorWithHexInit) { expectLogic31To0WithHexInit("val2", "32'h00000000"); }

// --- $display(":assert: (%d == N)", $onehot0(valN)); x 3 ---------------------

TEST_F(Onehot0FunctionTest, DisplayOnehot0OfVal0) { expectDisplayOfOnehot0(0, ":assert: (%d == 1)", "val0"); }

TEST_F(Onehot0FunctionTest, DisplayOnehot0OfVal1) { expectDisplayOfOnehot0(1, ":assert: (%d == 0)", "val1"); }

TEST_F(Onehot0FunctionTest, DisplayOnehot0OfVal2) { expectDisplayOfOnehot0(2, ":assert: (%d == 1)", "val2"); }

// --- compiler diagnostics -----------------------------------------------------

TEST_F(Onehot0FunctionTest, CompilesWithNoErrors) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
