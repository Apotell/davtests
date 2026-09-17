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

// Source under test: tests/Google/chapter-9/9.4.2.1--event_comma_op.sv
//
//   module block_tb ();
//   	wire a = 0;
//   	wire b = 0;
//   	wire c = 0;
//   	wire d = 0;
//   	reg out;
//   	always @(a, b, c, d)
//   		out = (a | b) & (c | d);
//   endmodule
//
// IEEE 1800-2023 clause tested: 9.4.2.1 "Event OR operator". The standard
// states the ',' operator can be used in an event_control's
// event_expression list in place of the keyword 'or' with identical
// meaning -- "always @(a, b, c, d)" is defined to be equivalent to
// "always @(a or b or c or d)". This file and
// 9.4.2.1--event_or_op.sv are therefore expected to elaborate to the same
// EventControl condition shape (each verified independently below, since
// CMake builds each test_*.cpp into its own executable).
//
// Checked:
//   - module block_tb exists.
//   - "a","b","c","d" are declared with the net-type keyword `wire`, so
//     per IEEE 1800-2023 6.7 they must be classified as hldb::Net.
//   - "out" is declared with the variable-type keyword `reg` (6.8), so it
//     must be classified as hldb::Variable, and -- since "reg out;" has
//     no initializer -- Variable::getExpr() must be null.
//   - "a"'s declared initializer (wire a = 0) is preserved either via
//     Net::getValue() or via a driving ContAssign on module->getContAssigns()
//     (IEEE 1800-2023 6.7.1: a net_decl_assignment is equivalent to an
//     implicit continuous assignment); the exact internal representation
//     is deliberately not pinned to one shape.
//   - exactly one Always process exists, getAlwaysType() == vpiAlways.
//   - the always block's EventControl condition is a single Operation
//     with opType == vpiEventOrOp and 4 operands, RefObj a,b,c,d in
//     declared order (comma is or-equivalent per 9.4.2.1, so no per-pair
//     nesting is expected).
//   - the controlled statement is the blocking Assignment
//     "out = (a | b) & (c | d)": Operation vpiBitAndOp whose two operands
//     are Operation vpiBitOrOp(a,b) and Operation vpiBitOrOp(c,d).
//
// Not checked:
//   - Runtime value of "out" after any input transition -- HLC is an
//     elaborator with no simulator (see .claude/hlc_overview.md).

#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/always.h>
#include <hldb/assignment.h>
#include <hldb/constant.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/event_control.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/variable.h>

namespace hlc {
class EventCommaOpTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "9.4.2.1--event_comma_op.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(EventCommaOpTest, ModuleExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("block_tb", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
}

TEST_F(EventCommaOpTest, ABCDAreNetsOutIsVariable) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("block_tb", m_design->getAllModules());
  ASSERT_NE(top, nullptr);

  for (const char *const name : {"a", "b", "c", "d"}) {
    const hldb::Net *const net = hldb::findByName<hldb::Net>(name, top->getNets());
    ASSERT_NE(net, nullptr) << name << " is declared with the net-type keyword 'wire' (IEEE 1800-2023 6.7)";
  }

  const hldb::Variable *const out = hldb::findByName<hldb::Variable>("out", top->getVariables());
  ASSERT_NE(out, nullptr) << "'out' is declared with the variable-type keyword 'reg' (IEEE 1800-2023 6.8)";
  EXPECT_EQ(out->getExpr(), nullptr) << "'reg out;' has no initializer";
}

TEST_F(EventCommaOpTest, AInitializerIsPreserved) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("block_tb", m_design->getAllModules());
  ASSERT_NE(top, nullptr);

  const hldb::Net *const a = hldb::findByName<hldb::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr);

  const hldb::Constant *directValue = a->getValue<hldb::Constant>();
  if (directValue == nullptr) {
    ASSERT_NE(top->getContAssigns(), nullptr) << "'wire a = 0' must produce either Net::getValue() "
                                                 "or an implicit ContAssign per IEEE 1800-2023 6.7.1";
    for (const hldb::ContAssign *const contAssign : *top->getContAssigns()) {
      const hldb::RefObj *const lhs = any_cast<hldb::RefObj>(contAssign->getLhs());
      if ((lhs != nullptr) && (lhs->getName() == "a")) {
        directValue = any_cast<hldb::Constant>(contAssign->getRhs());
        break;
      }
    }
  }
  ASSERT_NE(directValue, nullptr) << "'a's declared initializer (0) was not found via either representation";
  EXPECT_EQ(directValue->getDecompile(), "0");
}

TEST_F(EventCommaOpTest, ExactlyOnePlainAlwaysProcess) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("block_tb", m_design->getAllModules());
  ASSERT_NE(top, nullptr);

  const hldb::Always *always = nullptr;
  for (const hldb::Process *const process : *top->getProcesses()) {
    if (const hldb::Always *const candidate = any_cast<hldb::Always>(process)) {
      ASSERT_EQ(always, nullptr) << "expected exactly one always block in block_tb";
      always = candidate;
    }
  }
  ASSERT_NE(always, nullptr);
  EXPECT_EQ(always->getAlwaysType(), vpiAlways);
}

TEST_F(EventCommaOpTest, CommaSeparatedEventListIsEventOrOfFourOperands) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("block_tb", m_design->getAllModules());
  ASSERT_NE(top, nullptr);

  const hldb::Always *always = nullptr;
  for (const hldb::Process *const process : *top->getProcesses()) {
    if (const hldb::Always *const candidate = any_cast<hldb::Always>(process)) {
      always = candidate;
      break;
    }
  }
  ASSERT_NE(always, nullptr);

  const hldb::EventControl *const eventControl = any_cast<hldb::EventControl>(always->getStmt());
  ASSERT_NE(eventControl, nullptr) << "always @(a, b, c, d) must produce an EventControl";

  const hldb::Operation *const condition = any_cast<hldb::Operation>(eventControl->getCondition());
  ASSERT_NE(condition, nullptr) << "the ',' operator is defined (9.4.2.1) to be equivalent to 'or', "
                                   "so this must be a vpiEventOrOp Operation";
  EXPECT_EQ(condition->getOpType(), vpiEventOrOp);

  ASSERT_NE(condition->getOperands(), nullptr);
  ASSERT_EQ(condition->getOperands()->size(), 4u);
  const char *const expectedNames[4] = {"a", "b", "c", "d"};
  size_t index = 0;
  for (const hldb::Any *const operand : *condition->getOperands()) {
    const hldb::RefObj *const ref = any_cast<hldb::RefObj>(operand);
    ASSERT_NE(ref, nullptr);
    EXPECT_EQ(ref->getName(), expectedNames[index]);
    ++index;
  }
}

TEST_F(EventCommaOpTest, ControlledStatementIsOutAssignment) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("block_tb", m_design->getAllModules());
  ASSERT_NE(top, nullptr);

  const hldb::Always *always = nullptr;
  for (const hldb::Process *const process : *top->getProcesses()) {
    if (const hldb::Always *const candidate = any_cast<hldb::Always>(process)) {
      always = candidate;
      break;
    }
  }
  ASSERT_NE(always, nullptr);

  const hldb::EventControl *const eventControl = any_cast<hldb::EventControl>(always->getStmt());
  ASSERT_NE(eventControl, nullptr);

  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(eventControl->getStmt());
  ASSERT_NE(assign, nullptr) << "'out = (a | b) & (c | d);' must be a procedural Assignment";
  EXPECT_TRUE(assign->getBlocking());

  const hldb::RefObj *const lhs = any_cast<hldb::RefObj>(assign->getLhs());
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "out");

  const hldb::Operation *const rhs = any_cast<hldb::Operation>(assign->getRhs());
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getOpType(), vpiBitAndOp);
  ASSERT_NE(rhs->getOperands(), nullptr);
  ASSERT_EQ(rhs->getOperands()->size(), 2u);

  const hldb::Operation *const leftOr = any_cast<hldb::Operation>(rhs->getOperands()->at(0));
  ASSERT_NE(leftOr, nullptr);
  EXPECT_EQ(leftOr->getOpType(), vpiBitOrOp);
  ASSERT_NE(leftOr->getOperands(), nullptr);
  ASSERT_EQ(leftOr->getOperands()->size(), 2u);
  EXPECT_EQ(any_cast<hldb::RefObj>(leftOr->getOperands()->at(0))->getName(), "a");
  EXPECT_EQ(any_cast<hldb::RefObj>(leftOr->getOperands()->at(1))->getName(), "b");

  const hldb::Operation *const rightOr = any_cast<hldb::Operation>(rhs->getOperands()->at(1));
  ASSERT_NE(rightOr, nullptr);
  EXPECT_EQ(rightOr->getOpType(), vpiBitOrOp);
  ASSERT_NE(rightOr->getOperands(), nullptr);
  ASSERT_EQ(rightOr->getOperands()->size(), 2u);
  EXPECT_EQ(any_cast<hldb::RefObj>(rightOr->getOperands()->at(0))->getName(), "c");
  EXPECT_EQ(any_cast<hldb::RefObj>(rightOr->getOperands()->at(1))->getName(), "d");
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
