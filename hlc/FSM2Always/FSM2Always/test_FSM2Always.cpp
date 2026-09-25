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

// Tests for tests/FSM2Always/top.sv (module fsm_using_always)
//
//   module fsm_using_always (clock, reset, req_0, req_1, gnt_0, gnt_1);
//     input   clock,reset,req_0,req_1;
//     output  gnt_0,gnt_1;
//     wire    clock,reset,req_0,req_1;
//     reg     gnt_0,gnt_1;
//     parameter SIZE = 3;
//     parameter IDLE  = 3'b001,GNT0 = 3'b010,GNT1 = 3'b100;
//     reg [SIZE-1:0] state, next_state;
//     always @ (state or req_0 or req_1) begin : FSM_COMBO ... end
//     always @ (posedge clock)          begin : FSM_SEQ    ... end
//     always @ (posedge clock)          begin : OUTPUT_LOGIC ... end
//   endmodule
//
// This is the classic "FSM coded across two always blocks" style: one
// combinational always block computes next_state from state and inputs
// (IEEE 1800-2023 9.2.2.2 territory conceptually, though written here as a
// plain "always", not "always_comb"), and one clocked always block
// registers state on the clock edge (9.2.2.1-style sequential always).
// A third clocked always block drives outputs from state; it is checked
// only for existence/type, since the focal "two always blocks" pattern is
// FSM_COMBO + FSM_SEQ.
//
// What is checked (IEEE 1800-2023 citations):
//   - 6.7/6.8: "wire clock,reset,req_0,req_1" (net-type keyword) ->
//     4 Nets; "reg gnt_0,gnt_1"/"reg state,next_state" (no net-type
//     keyword) -> 4 Variables; no cross-duplication.
//   - 6.20.2 "Parameter declarations": SIZE/IDLE/GNT0/GNT1 are 4
//     Parameters, each with a ParamAssign carrying its Constant value
//     (SIZE=3 decimal -> vpiUIntConst "3"; IDLE/GNT0/GNT1 sized binary
//     literals -> vpiBinaryConst "3'b001"/"3'b010"/"3'b100").
//   - 9.2.1/9.4.2: module has exactly 3 Always processes.
//   - 9.4.2.1 "Event OR operator": FSM_COMBO's sensitivity list
//     "state or req_0 or req_1" is a single Operation(vpiEventOrOp) with
//     3 RefObj operands in declared order.
//   - 12.5.4/Annex 37.72: FSM_COMBO's body (named begin/end -> Begin with
//     EndLabel "FSM_COMBO") starts with a blocking Assignment
//     "next_state = 3'b000", followed by a CaseStmt on "state"
//     (vpiCaseExact, vpiNoQualifier) with 4 CaseItems (IDLE, GNT0, GNT1,
//     default); the IDLE item's statement is an IfElse (the
//     "if/else if/else" chain), and the default item assigns
//     "next_state = IDLE" via a RefObj resolving to the Parameter.
//   - 9.4.2 "posedge": FSM_SEQ's EventControl condition is
//     Operation(vpiPosedgeOp) with a single RefObj operand "clock"; its
//     body (Begin, EndLabel "FSM_SEQ") is an IfElse "if (reset == 1'b1)"
//     (Operation vpiEqOp) whose then-branch (Begin, 1 stmt) is the
//     non-blocking Assignment "state <= #1 IDLE" -- carrying a
//     DelayControl whose getDelay() is Constant "1" (11.5/9.7.3
//     intra-assignment delay on '<=').
//
// What is NOT checked and why:
//   - OUTPUT_LOGIC's full case/if structure -- it repeats the same
//     CaseStmt/IfElse shapes already verified via FSM_COMBO and FSM_SEQ;
//     only its existence and AlwaysType are checked, per the "meaningful
//     representative subset" guidance.
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

class FSM2AlwaysTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "FSM2Always.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getModule() {
    return hldb::findByName<hldb::Module>("fsm_using_always", m_design->getAllModules());
  }

  static const hldb::Parameter *getParameter(std::string_view name) {
    const hldb::Module *const mod = getModule();
    if (mod == nullptr) return nullptr;
    return hldb::findByName<hldb::Parameter>(name, mod->getParameters());
  }

  static const hldb::ParamAssign *getParamAssign(std::string_view name) {
    const hldb::Module *const mod = getModule();
    if (mod == nullptr) return nullptr;
    return hldb::findByName(name, mod->getParamAssigns());
  }

  // Named-begin bodies: each always block wraps its statement(s) in
  // "begin : LABEL ... end", so the always' stmt directly resolves to a
  // Begin whose EndLabel matches the source label.
  static const hldb::Begin *getNamedAlwaysBody(std::string_view label) {
    const hldb::Module *const mod = getModule();
    if (mod == nullptr || mod->getProcesses() == nullptr) return nullptr;
    for (const hldb::Process *const process : *mod->getProcesses()) {
      const hldb::Always *const always = any_cast<hldb::Always>(process);
      if (always == nullptr) continue;
      const hldb::EventControl *const ec = always->getStmt<hldb::EventControl>();
      if (ec == nullptr) continue;
      const hldb::Begin *const body = ec->getStmt<hldb::Begin>();
      if ((body != nullptr) && (body->getEndLabel() == label)) return body;
    }
    return nullptr;
  }

  static const hldb::EventControl *getEventControlFor(std::string_view label) {
    const hldb::Module *const mod = getModule();
    if (mod == nullptr || mod->getProcesses() == nullptr) return nullptr;
    for (const hldb::Process *const process : *mod->getProcesses()) {
      const hldb::Always *const always = any_cast<hldb::Always>(process);
      if (always == nullptr) continue;
      const hldb::EventControl *const ec = always->getStmt<hldb::EventControl>();
      if (ec == nullptr) continue;
      const hldb::Begin *const body = ec->getStmt<hldb::Begin>();
      if ((body != nullptr) && (body->getEndLabel() == label)) return ec;
    }
    return nullptr;
  }

  static const hldb::CaseStmt *getComboCaseStmt() {
    const hldb::Begin *const body = getNamedAlwaysBody("FSM_COMBO");
    if (body == nullptr || body->getStmts() == nullptr || body->getStmts()->size() < 2u) return nullptr;
    return any_cast<hldb::CaseStmt>(body->getStmts()->at(1));
  }
};

// --- module / declarations ----------------------------------------------

TEST_F(FSM2AlwaysTest, ModuleExists) { EXPECT_NE(getModule(), nullptr); }

TEST_F(FSM2AlwaysTest, FourNetsFourVariablesNoDuplication) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getNets(), nullptr);
  EXPECT_EQ(mod->getNets()->size(), 4u);
  for (std::string_view name : {"clock", "reset", "req_0", "req_1"}) {
    EXPECT_NE(hldb::findByName<hldb::Net>(name, mod->getNets()), nullptr) << name;
    EXPECT_EQ(hldb::findByName<hldb::Variable>(name, mod->getVariables()), nullptr)
        << name << " has a net-type keyword and must not also appear as a Variable";
  }
  ASSERT_NE(mod->getVariables(), nullptr);
  EXPECT_EQ(mod->getVariables()->size(), 4u);
  for (std::string_view name : {"gnt_0", "gnt_1", "state", "next_state"}) {
    EXPECT_NE(hldb::findByName<hldb::Variable>(name, mod->getVariables()), nullptr) << name;
    EXPECT_EQ(hldb::findByName<hldb::Net>(name, mod->getNets()), nullptr)
        << name << " has no net-type keyword and must not also appear as a Net";
  }
}

TEST_F(FSM2AlwaysTest, FourParametersWithExpectedValues) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getParamAssigns(), nullptr);
  EXPECT_EQ(mod->getParamAssigns()->size(), 4u);

  const hldb::Parameter *const size = getParameter("SIZE");
  ASSERT_NE(size, nullptr);
  const hldb::ParamAssign *const sizeAssign = getParamAssign("SIZE");
  ASSERT_NE(sizeAssign, nullptr);
  EXPECT_EQ(sizeAssign->getLhs<hldb::Parameter>(), size);
  ASSERT_NE(sizeAssign->getRhs(), nullptr);
  const hldb::Constant *const sizeVal = sizeAssign->getRhs<hldb::Constant>();
  ASSERT_NE(sizeVal, nullptr);
  EXPECT_EQ(sizeVal->getDecompile(), std::string_view{"3"});
  EXPECT_EQ(sizeVal->getConstType(), vpiUIntConst);

  const std::pair<std::string_view, std::string_view> expected[3] = {
      {"IDLE", "3'b001"},
      {"GNT0", "3'b010"},
      {"GNT1", "3'b100"},
  };
  for (const std::pair<std::string_view, std::string_view> &entry : expected) {
    const hldb::Parameter *const param = getParameter(entry.first);
    ASSERT_NE(param, nullptr) << entry.first;
    const hldb::ParamAssign *const assign = getParamAssign(entry.first);
    ASSERT_NE(assign, nullptr) << entry.first;
    EXPECT_EQ(assign->getLhs<hldb::Parameter>(), param) << entry.first;
    ASSERT_NE(assign->getRhs(), nullptr);
    const hldb::Constant *const value = assign->getRhs<hldb::Constant>();
    ASSERT_NE(value, nullptr) << entry.first;
    EXPECT_EQ(value->getDecompile(), entry.second);
    EXPECT_EQ(value->getConstType(), vpiBinaryConst);
  }
}

TEST_F(FSM2AlwaysTest, ExactlyThreeAlwaysProcesses) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getProcesses(), nullptr);
  EXPECT_EQ(mod->getProcesses()->size(), 3u);
  for (const hldb::Process *const process : *mod->getProcesses()) {
    const hldb::Always *const always = any_cast<hldb::Always>(process);
    ASSERT_NE(always, nullptr) << "every process in this module should be a plain 'always' block";
    EXPECT_EQ(always->getAlwaysType(), vpiAlways);
  }
}

// --- FSM_COMBO: always @ (state or req_0 or req_1) -----------------------

TEST_F(FSM2AlwaysTest, ComboSensitivityListIsEventOrOfThreeSignals) {
  const hldb::EventControl *const ec = getEventControlFor("FSM_COMBO");
  ASSERT_NE(ec, nullptr) << "FSM_COMBO body not found";
  ASSERT_NE(ec->getCondition(), nullptr);
  const hldb::Operation *const cond = ec->getCondition<hldb::Operation>();
  ASSERT_NE(cond, nullptr) << "'state or req_0 or req_1' should produce a single Operation";
  EXPECT_EQ(cond->getOpType(), vpiEventOrOp);
  ASSERT_NE(cond->getOperands(), nullptr);
  ASSERT_EQ(cond->getOperands()->size(), 3u);
  const std::string_view expectedNames[3] = {"state", "req_0", "req_1"};
  size_t index = 0;
  for (const hldb::Any *const operand : *cond->getOperands()) {
    ASSERT_NE(operand, nullptr);
    const hldb::RefObj *const ref = any_cast<hldb::RefObj>(operand);
    ASSERT_NE(ref, nullptr) << index;
    EXPECT_EQ(ref->getName(), expectedNames[index]);
    ++index;
  }
}

TEST_F(FSM2AlwaysTest, ComboBodyStartsWithDefaultAssignThenCase) {
  const hldb::Begin *const body = getNamedAlwaysBody("FSM_COMBO");
  ASSERT_NE(body, nullptr);
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 2u) << "'next_state = 3'b000;' then the case statement";

  const hldb::Assignment *const defaultAssign = any_cast<hldb::Assignment>(body->getStmts()->at(0));
  ASSERT_NE(defaultAssign, nullptr);
  EXPECT_TRUE(defaultAssign->getBlocking());
  const hldb::RefObj *const lhs = defaultAssign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), std::string_view{"next_state"});
  ASSERT_NE(defaultAssign->getRhs(), nullptr);
  const hldb::Constant *const rhs = defaultAssign->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), std::string_view{"3'b000"});

  EXPECT_NE(getComboCaseStmt(), nullptr) << "second statement should be the case(state) statement";
}

TEST_F(FSM2AlwaysTest, ComboCaseStmtHasFourItemsOnState) {
  const hldb::CaseStmt *const cs = getComboCaseStmt();
  ASSERT_NE(cs, nullptr);
  EXPECT_EQ(cs->getCaseType(), vpiCaseExact);
  EXPECT_EQ(cs->getQualifier(), vpiNoQualifier);
  ASSERT_NE(cs->getCondition(), nullptr);
  const hldb::RefObj *const cond = cs->getCondition<hldb::RefObj>();
  ASSERT_NE(cond, nullptr);
  EXPECT_EQ(cond->getName(), std::string_view{"state"});
  ASSERT_NE(cs->getCaseItems(), nullptr);
  EXPECT_EQ(cs->getCaseItems()->size(), 4u) << "IDLE, GNT0, GNT1, default";
}

TEST_F(FSM2AlwaysTest, ComboIdleCaseItemBodyIsIfElseChain) {
  const hldb::CaseStmt *const cs = getComboCaseStmt();
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

  const hldb::IfElse *const ifElse = idleItem->getStmt<hldb::IfElse>();
  ASSERT_NE(ifElse, nullptr) << "'if (req_0==1) ... else if (req_1==1) ... else ...' -> IfElse";
  EXPECT_NE(ifElse->getElseStmt(), nullptr) << "the trailing 'else if' chain must populate the else branch";
}

TEST_F(FSM2AlwaysTest, ComboDefaultCaseItemAssignsIdle) {
  const hldb::CaseStmt *const cs = getComboCaseStmt();
  ASSERT_NE(cs, nullptr);
  ASSERT_NE(cs->getCaseItems(), nullptr);
  ASSERT_GE(cs->getCaseItems()->size(), 4u);
  const hldb::CaseItem *const defaultItem = cs->getCaseItems()->at(3);
  ASSERT_NE(defaultItem, nullptr);
  EXPECT_EQ(defaultItem->getExprs(), nullptr) << "default case item must have no case_item_expression";

  const hldb::Assignment *const assign = defaultItem->getStmt<hldb::Assignment>();
  ASSERT_NE(assign, nullptr);
  EXPECT_TRUE(assign->getBlocking());
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), std::string_view{"next_state"});
  ASSERT_NE(assign->getRhs(), nullptr);
  const hldb::RefObj *const rhs = assign->getRhs<hldb::RefObj>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getName(), std::string_view{"IDLE"});
  EXPECT_EQ(rhs->getActual(), getParameter("IDLE"));
}

// --- FSM_SEQ: always @ (posedge clock) ------------------------------------

TEST_F(FSM2AlwaysTest, SeqEventControlIsPosedgeOfClock) {
  const hldb::EventControl *const ec = getEventControlFor("FSM_SEQ");
  ASSERT_NE(ec, nullptr) << "FSM_SEQ body not found";
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

TEST_F(FSM2AlwaysTest, SeqBodyIsResetIfElseWithDelayedNonBlockingAssigns) {
  const hldb::Begin *const body = getNamedAlwaysBody("FSM_SEQ");
  ASSERT_NE(body, nullptr);
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 1u);

  const hldb::IfElse *const ifElse = any_cast<hldb::IfElse>(body->getStmts()->at(0));
  ASSERT_NE(ifElse, nullptr) << "'if (reset == 1'b1) ... else ...' -> IfElse";

  ASSERT_NE(ifElse->getCondition(), nullptr);
  const hldb::Operation *const cond = ifElse->getCondition<hldb::Operation>();
  ASSERT_NE(cond, nullptr) << "'reset == 1'b1' should be Operation(vpiEqOp)";
  EXPECT_EQ(cond->getOpType(), vpiEqOp);
  ASSERT_NE(cond->getOperands(), nullptr);
  ASSERT_EQ(cond->getOperands()->size(), 2u);
  const hldb::RefObj *const condLhs = any_cast<hldb::RefObj>(cond->getOperands()->at(0));
  ASSERT_NE(condLhs, nullptr);
  EXPECT_EQ(condLhs->getName(), std::string_view{"reset"});

  const hldb::Begin *const thenBody = ifElse->getStmt<hldb::Begin>();
  ASSERT_NE(thenBody, nullptr);
  ASSERT_NE(thenBody->getStmts(), nullptr);
  ASSERT_EQ(thenBody->getStmts()->size(), 1u);
  const hldb::Assignment *const resetAssign = any_cast<hldb::Assignment>(thenBody->getStmts()->at(0));
  ASSERT_NE(resetAssign, nullptr) << "'state <= #1 IDLE;'";
  EXPECT_FALSE(resetAssign->getBlocking()) << "'<=' is non-blocking";
  const hldb::RefObj *const lhs = resetAssign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), std::string_view{"state"});
  ASSERT_NE(resetAssign->getRhs(), nullptr);
  const hldb::RefObj *const rhs = resetAssign->getRhs<hldb::RefObj>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getName(), std::string_view{"IDLE"});

  ASSERT_NE(resetAssign->getDelayControl(), nullptr) << "'#1' intra-assignment delay must be preserved";
  ASSERT_NE(resetAssign->getDelayControl()->getDelay(), nullptr);
  const hldb::Constant *const delay = resetAssign->getDelayControl()->getDelay<hldb::Constant>();
  ASSERT_NE(delay, nullptr);
  EXPECT_EQ(delay->getDecompile(), std::string_view{"1"});
}

// --- OUTPUT_LOGIC: existence only (structure repeats FSM_COMBO/FSM_SEQ) --

TEST_F(FSM2AlwaysTest, OutputLogicProcessExists) {
  EXPECT_NE(getNamedAlwaysBody("OUTPUT_LOGIC"), nullptr);
}

// --- compiler diagnostics -------------------------------------------------

TEST_F(FSM2AlwaysTest, NoFailedBinds) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND), nullptr)
      << "all references to state/next_state/req_0/req_1/clock/reset/parameters must bind";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
