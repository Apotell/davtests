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

// Source under test: tests/Google/chapter-9/9.4.2--event_control_edge.sv
//
//   module block_tb ();
//   	reg [3:0] a = 0;
//   	wire clk = 0;
//   	always @(edge clk) a = ~a;
//   endmodule
//
// IEEE 1800-2023 clause tested: 9.4.2 "Event control", edge_identifier ::=
// posedge | negedge | edge. The "edge" identifier matches any recognized
// value transition of its operand (it is not restricted to a single
// direction the way posedge/negedge are), so it is semantically distinct
// from both posedge and negedge while still being an edge-sensitive (not a
// bare level-sensitive) trigger.
//
// Checked:
//   - module block_tb exists.
//   - "clk" is declared with the net-type keyword `wire`, so per IEEE
//     1800-2023 6.7 it must be classified as hldb::Net (module->getNets()),
//     independent of hldb's own scope-based Net/Variable split.
//   - "a" is declared with the variable-type keyword `reg` (6.8's
//     data_type grammar), so per 6.7/6.8 it must be classified as
//     hldb::Variable (module->getVariables()) even though it is declared at
//     module scope -- this is the net/variable misclassification corner
//     documented in this project's memory; a mismatch here is a real,
//     non-skipped failing test, not something to quietly work around.
//   - exactly one Always process exists on the module, with
//     getAlwaysType() == vpiAlways (plain "always", not always_comb/ff/
//     latch).
//   - the always block's statement is an EventControl (9.4.2's
//     event_control) whose condition is non-null.
//   - if the condition is wrapped in an Operation (as posedge/negedge
//     always are), its single operand is a RefObj naming "clk" -- and,
//     per the semantic distinction above, its opType is neither
//     vpiPosedgeOp nor vpiNegedgeOp.
//   - the controlled statement is a blocking Assignment (`=`, not `<=`)
//     with lhs RefObj "a" and rhs an Operation (vpiBitNegOp, "~a") whose
//     single operand is RefObj "a".
//
// Not checked:
//   - The exact vpiXxxOp enumerant HLC assigns to identify the plain
//     "edge" qualifier itself. vpi_user.h/sv_vpi_user.h/hldb_vpi_user.h
//     (this repo's installed VPI headers) only define vpiPosedgeOp (39)
//     and vpiNegedgeOp (40) for edge_identifier -- there is no
//     corresponding standard enumerant for the bare "edge" keyword to
//     assert against without guessing an undocumented internal value.
//     Left as an open item until such a constant is confirmed.
//   - Runtime toggling behavior of "a" on a clk transition -- HLC is an
//     elaborator with no simulator (see .claude/hlc_overview.md), so no
//     post-elaboration/runtime value can be observed on the UHDM graph.

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
class EventControlEdgeTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "9.4.2--event_control_edge.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(EventControlEdgeTest, ModuleExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("block_tb", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
}

TEST_F(EventControlEdgeTest, ClkIsNetNotVariable) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("block_tb", m_design->getAllModules());
  ASSERT_NE(top, nullptr);

  const hldb::Net *const clk = hldb::findByName<hldb::Net>("clk", top->getNets());
  ASSERT_NE(clk, nullptr) << "'clk' is declared with the net-type keyword 'wire' (IEEE 1800-2023 6.7)";
}

TEST_F(EventControlEdgeTest, AIsVariableNotNet) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("block_tb", m_design->getAllModules());
  ASSERT_NE(top, nullptr);

  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr) << "'a' is declared with the variable-type keyword 'reg' (IEEE 1800-2023 6.8), "
                           "so it must be a Variable regardless of module scope";

  const hldb::Expr *const initExpr = a->getValue();
  ASSERT_NE(initExpr, nullptr) << "'reg [3:0] a = 0' has a variable_decl_assignment initializer";
  const hldb::Constant *const initValue = any_cast<hldb::Constant>(initExpr);
  ASSERT_NE(initValue, nullptr);
  EXPECT_EQ(initValue->getDecompile(), std::string_view("0"));
}

TEST_F(EventControlEdgeTest, ExactlyOnePlainAlwaysProcess) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("block_tb", m_design->getAllModules());
  ASSERT_NE(top, nullptr);

  const hldb::ProcessCollection *const processes = top->getProcesses();
  ASSERT_NE(processes, nullptr);

  const hldb::Always *always = nullptr;
  for (const hldb::Process *const process : *processes) {
    if (const hldb::Always *const candidate = any_cast<hldb::Always>(process)) {
      ASSERT_EQ(always, nullptr) << "expected exactly one always block in block_tb";
      always = candidate;
    }
  }
  ASSERT_NE(always, nullptr);
  EXPECT_EQ(always->getAlwaysType(), vpiAlways);
}

TEST_F(EventControlEdgeTest, EdgeEventControlConditionReferencesClk) {
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
  ASSERT_NE(eventControl, nullptr) << "always @(edge clk) must produce an EventControl (9.4.2 event_control)";

  const hldb::Any *const condition = eventControl->getCondition();
  ASSERT_NE(condition, nullptr);

  if (const hldb::Operation *const op = any_cast<hldb::Operation>(condition)) {
    EXPECT_NE(op->getOpType(), vpiPosedgeOp) << "'edge' matches both transitions, unlike 'posedge'";
    EXPECT_NE(op->getOpType(), vpiNegedgeOp) << "'edge' matches both transitions, unlike 'negedge'";

    ASSERT_NE(op->getOperands(), nullptr);
    ASSERT_EQ(op->getOperands()->size(), 1u);
    const hldb::RefObj *const operand = any_cast<hldb::RefObj>(op->getOperands()->front());
    ASSERT_NE(operand, nullptr);
    EXPECT_EQ(operand->getName(), "clk");
  } else {
    const hldb::RefObj *const asRef = any_cast<hldb::RefObj>(condition);
    ASSERT_NE(asRef, nullptr) << "condition must be either an edge Operation or a direct RefObj to clk";
    EXPECT_EQ(asRef->getName(), "clk");
  }
}

TEST_F(EventControlEdgeTest, ControlledStatementIsBlockingBitNegAssignment) {
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
  ASSERT_NE(assign, nullptr) << "'a = ~a;' must be a procedural Assignment";
  EXPECT_TRUE(assign->getBlocking()) << "'=' is a blocking assignment";

  const hldb::RefObj *const lhs = any_cast<hldb::RefObj>(assign->getLhs());
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "a");

  const hldb::Operation *const rhs = any_cast<hldb::Operation>(assign->getRhs());
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getOpType(), vpiBitNegOp);
  ASSERT_NE(rhs->getOperands(), nullptr);
  ASSERT_EQ(rhs->getOperands()->size(), 1u);
  const hldb::RefObj *const rhsOperand = any_cast<hldb::RefObj>(rhs->getOperands()->front());
  ASSERT_NE(rhsOperand, nullptr);
  EXPECT_EQ(rhsOperand->getName(), "a");
}

TEST_F(EventControlEdgeTest, RuntimeToggleShouldFlipAButHlcCannotSimulate) {
  GTEST_SKIP() << "HLC has no simulator (see .claude/hlc_overview.md); the effect of a 'clk' "
                  "transition toggling 'a' via '~a' can only be observed at runtime, not on "
                  "the elaborated UHDM graph.";

  const hldb::Module *const top = hldb::findByName<hldb::Module>("block_tb", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  const hldb::Constant *const value = a->getValue<hldb::Constant>();
  ASSERT_NE(value, nullptr) << "post-simulation value of 'a' is never populated at compile time";
  EXPECT_NE(value->getDecompile(), "0") << "after one clk transition 'a' should have toggled away from 0";
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
