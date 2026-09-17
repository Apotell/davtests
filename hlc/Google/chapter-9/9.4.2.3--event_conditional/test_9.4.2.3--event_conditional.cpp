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

// Source under test: tests/Google/chapter-9/9.4.2.3--event_conditional.sv
//
//   module block_tb ();
//   	wire clk = 0;
//   	wire en = 0;
//   	wire a = 0;
//   	reg y;
//   	always @(posedge clk iff en == 1)
//   		y <= a;
//   endmodule
//
// IEEE 1800-2023 clause tested: 9.4.2 event_expression ::= ... |
// event_expression 'iff' expression. The 'iff' qualifier gates an edge
// event with a boolean guard: the always block only triggers on a posedge
// of clk that occurs while "en == 1" also holds. This introduces a second
// level of nesting beyond a plain posedge/negedge/edge event: the
// EventControl condition itself becomes a vpiIffOp Operation whose two
// operands are the qualified event (posedge clk) and the guard expression
// (en == 1).
//
// Checked:
//   - module block_tb exists.
//   - "clk","en","a" are declared with the net-type keyword `wire`, so
//     per IEEE 1800-2023 6.7 they must be classified as hldb::Net.
//   - "y" is declared with the variable-type keyword `reg` (6.8), so it
//     must be classified as hldb::Variable, with no initializer.
//   - exactly one Always process exists, getAlwaysType() == vpiAlways.
//   - the always block's EventControl condition is an Operation with
//     opType == vpiIffOp and exactly 2 operands:
//       operand[0]: Operation vpiPosedgeOp with a single RefObj "clk"
//                   operand (the qualified event),
//       operand[1]: Operation vpiEqOp with operands RefObj "en" and
//                   Constant "1" (the guard expression).
//   - the controlled statement "y <= a;" is a nonblocking Assignment
//     (getBlocking() == false) with lhs RefObj "y" and rhs a plain RefObj
//     "a" (no operation -- it is a bare reference, not an expression).
//
// Not checked:
//   - Runtime value of "y" after any clk/en transition -- HLC is an
//     elaborator with no simulator (see .claude/hlc_overview.md).

#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/always.h>
#include <hldb/assignment.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/event_control.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/variable.h>

namespace hlc {
class EventConditionalTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "9.4.2.3--event_conditional.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(EventConditionalTest, ModuleExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("block_tb", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
}

TEST_F(EventConditionalTest, ClkEnAAreNetsYIsVariable) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("block_tb", m_design->getAllModules());
  ASSERT_NE(top, nullptr);

  for (const char *const name : {"clk", "en", "a"}) {
    const hldb::Net *const net = hldb::findByName<hldb::Net>(name, top->getNets());
    ASSERT_NE(net, nullptr) << name << " is declared with the net-type keyword 'wire' (IEEE 1800-2023 6.7)";
  }

  const hldb::Variable *const y = hldb::findByName<hldb::Variable>("y", top->getVariables());
  ASSERT_NE(y, nullptr) << "'y' is declared with the variable-type keyword 'reg' (IEEE 1800-2023 6.8)";
  EXPECT_EQ(y->getExpr(), nullptr) << "'reg y;' has no initializer";
}

TEST_F(EventConditionalTest, ExactlyOnePlainAlwaysProcess) {
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

TEST_F(EventConditionalTest, IffGatesPosedgeClkWithEnEqualsOne) {
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
  ASSERT_NE(eventControl, nullptr) << "always @(posedge clk iff en == 1) must produce an EventControl";

  const hldb::Operation *const posedgeOp = eventControl->getCondition<hldb::Operation>();
  ASSERT_NE(posedgeOp, nullptr) << "first 'iff' operand must be the qualified posedge event";
  EXPECT_EQ(posedgeOp->getOpType(), vpiPosedgeOp);
  ASSERT_NE(posedgeOp->getOperands(), nullptr);
  ASSERT_EQ(posedgeOp->getOperands()->size(), 2u);
  const hldb::RefObj *const clkRef = any_cast<hldb::RefObj>(posedgeOp->getOperands()->front());
  ASSERT_NE(clkRef, nullptr);
  EXPECT_EQ(clkRef->getName(), "clk");

  const hldb::Operation *const iffOp = any_cast<hldb::Operation>(posedgeOp->getOperands()->back());
  ASSERT_NE(iffOp, nullptr) << "'iff' must produce a vpiIffOp Operation gating the qualified event";
  EXPECT_EQ(iffOp->getOpType(), vpiIffOp);
  ASSERT_NE(iffOp->getOperands(), nullptr);
  ASSERT_EQ(iffOp->getOperands()->size(), 1u);

  const hldb::Operation *const guardOp = any_cast<hldb::Operation>(iffOp->getOperands()->front());
  ASSERT_NE(guardOp, nullptr) << "second 'iff' operand must be the guard expression 'en == 1'";
  EXPECT_EQ(guardOp->getOpType(), vpiEqOp);
  ASSERT_NE(guardOp->getOperands(), nullptr);
  ASSERT_EQ(guardOp->getOperands()->size(), 2u);
  const hldb::RefObj *const enRef = any_cast<hldb::RefObj>(guardOp->getOperands()->at(0));
  ASSERT_NE(enRef, nullptr);
  EXPECT_EQ(enRef->getName(), "en");
  const hldb::Constant *const one = any_cast<hldb::Constant>(guardOp->getOperands()->at(1));
  ASSERT_NE(one, nullptr);
  EXPECT_EQ(one->getDecompile(), "1");
}

TEST_F(EventConditionalTest, ControlledStatementIsNonblockingYAssignment) {
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
  ASSERT_NE(assign, nullptr) << "'y <= a;' must be a procedural Assignment";
  EXPECT_FALSE(assign->getBlocking()) << "'<=' is a nonblocking assignment";

  const hldb::RefObj *const lhs = any_cast<hldb::RefObj>(assign->getLhs());
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "y");

  const hldb::RefObj *const rhs = any_cast<hldb::RefObj>(assign->getRhs());
  ASSERT_NE(rhs, nullptr) << "'a' on the rhs is a bare reference, not an operation";
  EXPECT_EQ(rhs->getName(), "a");
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
