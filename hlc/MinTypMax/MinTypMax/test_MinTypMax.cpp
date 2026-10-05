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

// Tests for tests/MinTypMax/dut.sv (tags: MinTypMax)
//
//   module top;
//     parameter value = (1:2:3);
//     initial $display(value);
//   endmodule
//
// What is checked:
//   - module "top" declares the non-local value parameter "value"
//     (Sec 6.20.2)
//   - Sec 11.11 / A.8.4: "constant_primary ::= ( constant_mintypmax_expression )"
//     and "constant_mintypmax_expression ::= constant_expression :
//     constant_expression : constant_expression", so the default value
//     "(1:2:3)" is a single min:typ:max expression: Operation
//     (vpiMinTypMaxOp) with exactly 3 operands -- the Constants 1, 2, 3 in
//     min, typ, max order
//   - Sec 9.2.1 / 21.2.1: the initial procedure's statement is directly
//     the system task call "$display" (SysTaskCall) with one argument, a
//     RefObj "value" bound to the Parameter "value"
//   - no COMP_FAILED_TO_BIND for "value"
//
// What is NOT checked and why:
//   - Which of min/typ/max is selected as the parameter's value (typical
//     by default, or as chosen by a tool option, Sec 11.11): that
//     selection is made when the expression is evaluated, at elaboration;
//     this compile stops before elaboration.
//   - The parameter's resulting implicit type (Sec 6.20.2: takes the type
//     of the final value): also an elaboration result.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/initial.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/ref_obj.h>
#include <hldb/sys_task_call.h>
#include <hldb/vpi_user.h>

namespace hlc {

class MinTypMaxTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "MinTypMax.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByDefName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::Parameter *getValueParam() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getParameters() == nullptr) return nullptr;
    for (const hldb::Any *const p : *top->getParameters()) {
      const hldb::Parameter *const param = any_cast<hldb::Parameter>(p);
      if (param != nullptr && param->getName() == "value") return param;
    }
    return nullptr;
  }

  static const hldb::Operation *getMinTypMax() {
    const hldb::Module *const top = getTop();
    if (top == nullptr) return nullptr;
    const hldb::ParamAssign *const pa = hldb::findByName<hldb::ParamAssign>("value", hldb::getParamAssigns(top));
    return (pa == nullptr) ? nullptr : pa->getRhs<hldb::Operation>();
  }

  static const hldb::SysTaskCall *getDisplay() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getProcesses() == nullptr) return nullptr;
    for (const hldb::Process *const p : *top->getProcesses()) {
      if (const hldb::Initial *const init = any_cast<hldb::Initial>(p)) return init->getStmt<hldb::SysTaskCall>();
    }
    return nullptr;
  }
};

TEST_F(MinTypMaxTest, TopDeclaresParameterValue) {
  ASSERT_NE(getTop(), nullptr) << "module 'top' not found";
  const hldb::Parameter *const p = getValueParam();
  ASSERT_NE(p, nullptr) << "'parameter value' not found";
  EXPECT_FALSE(p->getLocalParam());
}

TEST_F(MinTypMaxTest, DefaultIsMinTypMaxOperation) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::ParamAssign *const pa = hldb::findByName<hldb::ParamAssign>("value", hldb::getParamAssigns(top));
  ASSERT_NE(pa, nullptr) << "ParamAssign for 'value' not found";
  ASSERT_NE(pa->getRhs(), nullptr) << "'value' has a default value";
  const hldb::Operation *const op = getMinTypMax();
  ASSERT_NE(op, nullptr) << "'(1:2:3)' must be an Operation";
  EXPECT_EQ(op->getOpType(), vpiMinTypMaxOp) << "Sec 11.11: min:typ:max expression";
}

TEST_F(MinTypMaxTest, MinTypMaxHasThreeOperandsInOrder) {
  const hldb::Operation *const op = getMinTypMax();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_EQ(op->getOperands()->size(), 3u) << "min, typ, max";
  const char *const expected[] = {"1", "2", "3"};
  for (size_t i = 0; i < 3; ++i) {
    const hldb::Constant *const c = any_cast<hldb::Constant>(op->getOperands()->at(i));
    ASSERT_NE(c, nullptr) << "operand #" << i << " must be a Constant";
    EXPECT_EQ(c->getDecompile(), expected[i]) << "operand #" << i;
  }
}

TEST_F(MinTypMaxTest, InitialIsDisplayOfValue) {
  const hldb::SysTaskCall *const call = getDisplay();
  ASSERT_NE(call, nullptr) << "initial statement must be the system task call '$display(value)'";
  EXPECT_EQ(call->getName(), "$display");
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 1u);
  const hldb::RefObj *const arg = any_cast<hldb::RefObj>(call->getArguments()->at(0));
  ASSERT_NE(arg, nullptr);
  EXPECT_EQ(arg->getName(), "value");
  ASSERT_NE(getValueParam(), nullptr);
  EXPECT_EQ(arg->getActual(), getValueParam()) << "'value' must bind to the Parameter";
}

TEST_F(MinTypMaxTest, NoBindErrorForValue) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "value"), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
