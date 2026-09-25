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

// Tests for tests/FSMSingleAlways/top.sv (module fsm_using_single_always)
//
//   module fsm_using_single_always (clock, reset, req_0, req_1, gnt_0, gnt_1);
//     input   clock,reset,req_0,req_1;
//     output  gnt_0,gnt_1;
//     wire    clock,reset,req_0,req_1;
//     reg     gnt_0,gnt_1;
//     parameter SIZE = 3;
//     parameter IDLE  = 3'b001,GNT0 = 3'b010,GNT1 = 3'b100;
//     reg [SIZE-1:0] state, next_state;
//     always @ (posedge clock) begin : FSM
//       if (reset == 1'b1) begin
//         state <= #1 IDLE; gnt_0 <= 0; gnt_1 <= 0;
//       end else
//         case(state)
//           IDLE : if (req_0 == 1'b1) begin state <= #1 GNT0; gnt_0 <= 1; end
//                  else if (req_1 == 1'b1) begin gnt_1 <= 1; state <= #1 GNT1; end
//                  else begin state <= #1 IDLE; end
//           ...
//           default : state <= #1 IDLE;
//         endcase
//     end
//   endmodule
//
// This is the "single always block" FSM style: unlike FSM2Always (separate
// combinational/sequential always blocks), both next-state computation and
// state/output registration live in one clocked always block, with state
// transitions and output updates interleaved as non-blocking assignments
// inside the same case-item branches.
//
// What is checked (IEEE 1800-2023 citations):
//   - 6.7/6.8: same net/variable split as FSM2Always -- 4 Nets
//     (clock,reset,req_0,req_1), 4 Variables (gnt_0,gnt_1,state,next_state).
//   - 9.4.2: exactly 1 Always process, AlwaysType vpiAlways, EventControl
//     condition Operation(vpiPosedgeOp) over RefObj "clock"; body is a
//     Begin with EndLabel "FSM" containing exactly 1 statement.
//   - 12.4/12.4.1: that statement is an IfElse "if (reset == 1'b1) ...
//     else ..."; the then-branch (Begin, 3 stmts) resets state to IDLE
//     (delayed, non-blocking) and clears both outputs (non-delayed,
//     non-blocking); the else-branch is directly a CaseStmt (single
//     statement, no begin/end needed around it) with condition RefObj
//     "state" and 4 CaseItems (IDLE, GNT0, GNT1, default).
//   - The IDLE case item's body is itself an IfElse whose then-branch
//     (Begin, 2 stmts) shows the two different assignment-delay shapes
//     coexisting: "state <= #1 GNT0;" carries a DelayControl (Constant
//     "1"), while "gnt_0 <= 1;" carries none -- both non-blocking.
//   - the default item has no case_item_expression and assigns
//     "state <= #1 IDLE" (delayed, non-blocking, RefObj "IDLE" resolving
//     to the Parameter).
//
// What is NOT checked and why:
//   - The GNT0/GNT1 case items -- same IfElse/Assignment shapes already
//     verified via IDLE and default, per the "representative subset"
//     guidance.
//   - Runtime FSM transition behavior -- HLC is an elaborator with no
//     simulator.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/always.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/case_item.h>
#include <hldb/case_stmt.h>
#include <hldb/constant.h>
#include <hldb/delay_control.h>
#include <hldb/design.h>
#include <hldb/event_control.h>
#include <hldb/if_else.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/ref_obj.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class FSMSingleAlwaysTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "FSMSingleAlways.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getModule() {
    return hldb::findByName<hldb::Module>("fsm_using_single_always", m_design->getAllModules());
  }

  static const hldb::Parameter *getParameter(std::string_view name) {
    const hldb::Module *const mod = getModule();
    if (mod == nullptr) return nullptr;
    return hldb::findByName<hldb::Parameter>(name, mod->getParameters());
  }

  static const hldb::Always *getAlwaysProcess() {
    const hldb::Module *const mod = getModule();
    if (mod == nullptr || mod->getProcesses() == nullptr || mod->getProcesses()->empty()) return nullptr;
    return any_cast<hldb::Always>(mod->getProcesses()->at(0));
  }

  static const hldb::Begin *getBody() {
    const hldb::Always *const always = getAlwaysProcess();
    if (always == nullptr) return nullptr;
    const hldb::EventControl *const ec = always->getStmt<hldb::EventControl>();
    if (ec == nullptr) return nullptr;
    return ec->getStmt<hldb::Begin>();
  }

  static const hldb::IfElse *getResetIfElse() {
    const hldb::Begin *const body = getBody();
    if (body == nullptr || body->getStmts() == nullptr || body->getStmts()->empty()) return nullptr;
    return any_cast<hldb::IfElse>(body->getStmts()->at(0));
  }

  static const hldb::CaseStmt *getCaseStmt() {
    const hldb::IfElse *const ifElse = getResetIfElse();
    if (ifElse == nullptr) return nullptr;
    return ifElse->getElseStmt<hldb::CaseStmt>();
  }
};

// --- module / declarations ----------------------------------------------

TEST_F(FSMSingleAlwaysTest, ModuleExists) { EXPECT_NE(getModule(), nullptr); }

TEST_F(FSMSingleAlwaysTest, FourNetsFourVariablesNoDuplication) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getNets(), nullptr);
  EXPECT_EQ(mod->getNets()->size(), 4u);
  for (std::string_view name : {"clock", "reset", "req_0", "req_1"}) {
    EXPECT_NE(hldb::findByName<hldb::Net>(name, mod->getNets()), nullptr) << name;
  }
  ASSERT_NE(mod->getVariables(), nullptr);
  EXPECT_EQ(mod->getVariables()->size(), 4u);
  for (std::string_view name : {"gnt_0", "gnt_1", "state", "next_state"}) {
    EXPECT_NE(hldb::findByName<hldb::Variable>(name, mod->getVariables()), nullptr) << name;
  }
}

TEST_F(FSMSingleAlwaysTest, FourParametersExist) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getParamAssigns(), nullptr);
  EXPECT_EQ(mod->getParamAssigns()->size(), 4u);
  for (std::string_view name : {"SIZE", "IDLE", "GNT0", "GNT1"}) {
    EXPECT_NE(getParameter(name), nullptr) << name;
  }
}

TEST_F(FSMSingleAlwaysTest, ExactlyOneAlwaysProcessPosedgeClock) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getProcesses(), nullptr);
  EXPECT_EQ(mod->getProcesses()->size(), 1u);

  const hldb::Always *const always = getAlwaysProcess();
  ASSERT_NE(always, nullptr);
  EXPECT_EQ(always->getAlwaysType(), vpiAlways);

  const hldb::EventControl *const ec = always->getStmt<hldb::EventControl>();
  ASSERT_NE(ec, nullptr);
  ASSERT_NE(ec->getCondition(), nullptr);
  const hldb::Operation *const cond = ec->getCondition<hldb::Operation>();
  ASSERT_NE(cond, nullptr);
  EXPECT_EQ(cond->getOpType(), vpiPosedgeOp);
  ASSERT_NE(cond->getOperands(), nullptr);
  ASSERT_EQ(cond->getOperands()->size(), 1u);
  const hldb::RefObj *const operand = any_cast<hldb::RefObj>(cond->getOperands()->front());
  ASSERT_NE(operand, nullptr);
  EXPECT_EQ(operand->getName(), std::string_view{"clock"});
}

TEST_F(FSMSingleAlwaysTest, BodyIsNamedBeginWithSingleIfElse) {
  const hldb::Begin *const body = getBody();
  ASSERT_NE(body, nullptr);
  EXPECT_EQ(body->getEndLabel(), std::string_view{"FSM"});
  ASSERT_NE(body->getStmts(), nullptr);
  EXPECT_EQ(body->getStmts()->size(), 1u);
  EXPECT_NE(getResetIfElse(), nullptr);
}

// --- reset branch: state <= #1 IDLE; gnt_0 <= 0; gnt_1 <= 0; -------------

TEST_F(FSMSingleAlwaysTest, ResetConditionComparesResetToOne) {
  const hldb::IfElse *const ifElse = getResetIfElse();
  ASSERT_NE(ifElse, nullptr);
  ASSERT_NE(ifElse->getCondition(), nullptr);
  const hldb::Operation *const cond = ifElse->getCondition<hldb::Operation>();
  ASSERT_NE(cond, nullptr);
  EXPECT_EQ(cond->getOpType(), vpiEqOp);
  ASSERT_NE(cond->getOperands(), nullptr);
  ASSERT_EQ(cond->getOperands()->size(), 2u);
  const hldb::RefObj *const lhs = any_cast<hldb::RefObj>(cond->getOperands()->at(0));
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), std::string_view{"reset"});
}

TEST_F(FSMSingleAlwaysTest, ResetThenBranchHasThreeNonBlockingAssigns) {
  const hldb::IfElse *const ifElse = getResetIfElse();
  ASSERT_NE(ifElse, nullptr);
  const hldb::Begin *const thenBody = ifElse->getStmt<hldb::Begin>();
  ASSERT_NE(thenBody, nullptr);
  ASSERT_NE(thenBody->getStmts(), nullptr);
  ASSERT_EQ(thenBody->getStmts()->size(), 3u);

  const hldb::Assignment *const stateAssign = any_cast<hldb::Assignment>(thenBody->getStmts()->at(0));
  ASSERT_NE(stateAssign, nullptr);
  EXPECT_FALSE(stateAssign->getBlocking());
  ASSERT_NE(stateAssign->getDelayControl(), nullptr) << "'state <= #1 IDLE;' carries an intra-assignment delay";
  ASSERT_NE(stateAssign->getDelayControl()->getDelay(), nullptr);
  const hldb::Constant *const delay = stateAssign->getDelayControl()->getDelay<hldb::Constant>();
  ASSERT_NE(delay, nullptr);
  EXPECT_EQ(delay->getDecompile(), std::string_view{"1"});

  const hldb::Assignment *const gnt0Assign = any_cast<hldb::Assignment>(thenBody->getStmts()->at(1));
  ASSERT_NE(gnt0Assign, nullptr);
  EXPECT_FALSE(gnt0Assign->getBlocking());
  EXPECT_EQ(gnt0Assign->getDelayControl(), nullptr) << "'gnt_0 <= 0;' has no intra-assignment delay";
  const hldb::RefObj *const gnt0Lhs = gnt0Assign->getLhs<hldb::RefObj>();
  ASSERT_NE(gnt0Lhs, nullptr);
  EXPECT_EQ(gnt0Lhs->getName(), std::string_view{"gnt_0"});

  const hldb::Assignment *const gnt1Assign = any_cast<hldb::Assignment>(thenBody->getStmts()->at(2));
  ASSERT_NE(gnt1Assign, nullptr);
  EXPECT_FALSE(gnt1Assign->getBlocking());
  const hldb::RefObj *const gnt1Lhs = gnt1Assign->getLhs<hldb::RefObj>();
  ASSERT_NE(gnt1Lhs, nullptr);
  EXPECT_EQ(gnt1Lhs->getName(), std::string_view{"gnt_1"});
}

// --- else branch: case(state) ... endcase, held directly (no begin/end) --

TEST_F(FSMSingleAlwaysTest, ElseBranchIsDirectlyCaseStmtOnState) {
  const hldb::CaseStmt *const cs = getCaseStmt();
  ASSERT_NE(cs, nullptr) << "single-statement else-branch (no begin/end) must not be wrapped in a Begin";
  EXPECT_EQ(cs->getCaseType(), vpiCaseExact);
  ASSERT_NE(cs->getCondition(), nullptr);
  const hldb::RefObj *const cond = cs->getCondition<hldb::RefObj>();
  ASSERT_NE(cond, nullptr);
  EXPECT_EQ(cond->getName(), std::string_view{"state"});
  ASSERT_NE(cs->getCaseItems(), nullptr);
  EXPECT_EQ(cs->getCaseItems()->size(), 4u) << "IDLE, GNT0, GNT1, default";
}

TEST_F(FSMSingleAlwaysTest, IdleCaseItemMixesDelayedAndUndelayedNonBlockingAssigns) {
  const hldb::CaseStmt *const cs = getCaseStmt();
  ASSERT_NE(cs, nullptr);
  ASSERT_NE(cs->getCaseItems(), nullptr);
  ASSERT_GE(cs->getCaseItems()->size(), 1u);
  const hldb::CaseItem *const idleItem = cs->getCaseItems()->at(0);
  ASSERT_NE(idleItem, nullptr);
  ASSERT_NE(idleItem->getExprs(), nullptr);
  ASSERT_EQ(idleItem->getExprs()->size(), 1u);
  const hldb::RefObj *const label = any_cast<hldb::RefObj>(idleItem->getExprs()->at(0));
  ASSERT_NE(label, nullptr);
  EXPECT_EQ(label->getName(), std::string_view{"IDLE"});

  const hldb::IfElse *const inner = idleItem->getStmt<hldb::IfElse>();
  ASSERT_NE(inner, nullptr) << "'if (req_0==1) ... else if (req_1==1) ... else ...' -> IfElse";

  const hldb::Begin *const thenBody = inner->getStmt<hldb::Begin>();
  ASSERT_NE(thenBody, nullptr);
  ASSERT_NE(thenBody->getStmts(), nullptr);
  ASSERT_EQ(thenBody->getStmts()->size(), 2u);

  const hldb::Assignment *const stateAssign = any_cast<hldb::Assignment>(thenBody->getStmts()->at(0));
  ASSERT_NE(stateAssign, nullptr) << "'state <= #1 GNT0;'";
  EXPECT_FALSE(stateAssign->getBlocking());
  EXPECT_NE(stateAssign->getDelayControl(), nullptr) << "carries the '#1' intra-assignment delay";

  const hldb::Assignment *const gntAssign = any_cast<hldb::Assignment>(thenBody->getStmts()->at(1));
  ASSERT_NE(gntAssign, nullptr) << "'gnt_0 <= 1;'";
  EXPECT_FALSE(gntAssign->getBlocking());
  EXPECT_EQ(gntAssign->getDelayControl(), nullptr) << "no '#1' on this assignment";
}

TEST_F(FSMSingleAlwaysTest, DefaultCaseItemAssignsIdleWithDelay) {
  const hldb::CaseStmt *const cs = getCaseStmt();
  ASSERT_NE(cs, nullptr);
  ASSERT_NE(cs->getCaseItems(), nullptr);
  ASSERT_GE(cs->getCaseItems()->size(), 4u);
  const hldb::CaseItem *const defaultItem = cs->getCaseItems()->at(3);
  ASSERT_NE(defaultItem, nullptr);
  EXPECT_EQ(defaultItem->getExprs(), nullptr) << "default case item must have no case_item_expression";

  const hldb::Assignment *const assign = defaultItem->getStmt<hldb::Assignment>();
  ASSERT_NE(assign, nullptr) << "'default : state <= #1 IDLE;'";
  EXPECT_FALSE(assign->getBlocking());
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), std::string_view{"state"});
  ASSERT_NE(assign->getRhs(), nullptr);
  const hldb::RefObj *const rhs = assign->getRhs<hldb::RefObj>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getName(), std::string_view{"IDLE"});
  EXPECT_EQ(rhs->getActual(), getParameter("IDLE"));
  ASSERT_NE(assign->getDelayControl(), nullptr);
}

// --- compiler diagnostics -------------------------------------------------

TEST_F(FSMSingleAlwaysTest, NoFailedBinds) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND), nullptr)
      << "all references to state/gnt_0/gnt_1/req_0/req_1/clock/reset/parameters must bind";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
