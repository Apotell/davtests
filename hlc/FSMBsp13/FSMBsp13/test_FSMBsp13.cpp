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

// Tests for tests/FSMBsp13/top.v, fsm1.v, fsm2.v, fsm3.v (FSMBsp13.hlc
// compiles all four files together).
//
//   module top;
//     ...
//     FSM1 F1(.Clk(fsm1clk), .Reset(fsm1rst), .SlowRam(SlowRam),
//             .Read(rd), .Write(wr));
//     FSM2 F2(.clock(fsm2clk), .reset(fsm2rst), .control(ctrl), .y(Fsm2Out));
//     FSM3 F3(.clock(fsm3clk), .keys(keys), .brake(brake),
//             .accelerate(accelerate), .Speed(speed));
//     ...
//   endmodule
//
//   module FSM1 (Clk, Reset, SlowRam, Read, Write);
//     parameter [3:0] ST_Read=0, ST_Write=1, ST_Delay=2, ST_Trx=3,
//       ST_Hold=4, ST_Block=5, ST_Wait=6, ST_Turn=7, ST_Quit=8, ST_Exit=9,
//       ST_Done=10;
//     reg [3:0] CurState, NextState;
//     always @(posedge Clk) begin:SEQ
//       if (Reset) CurState = ST_Read; else CurState = NextState;
//     end
//     initial begin Read = 0; Write = 1; Wait = 0; Delay = 1; end
//     always @(CurState) begin:COMB
//       case (CurState)
//         ST_Trx: begin ... end
//         ...
//         ST_Read: begin Read=1; Write=0; ...; NextState=ST_Write; end
//         ...
//         default: begin ... end
//       endcase
//     end
//   endmodule
//
// "top" is a plain testbench (no synthesizable logic of its own); the
// actual FSM regression content is in the 3 instantiated modules. This
// file checks top's instance hierarchy (module-instance -> defName
// mapping) and then focuses the FSM-shape checks on FSM1, whose
// SEQ/COMB pair is the same "two always blocks" style as FSM2Always, but
// with named (not anonymous/localparam) width-annotated parameters and a
// larger (11-state) case statement -- a different real-world coding
// variant worth its own coverage.
//
// What is checked (IEEE 1800-2023 citations):
//   - 23.3 "Module instantiation": the (unelaborated) definition "top"
//     has exactly 3 instantiations (getRefInstances()), F1/F2/F3, whose
//     ModuleTypespec resolves to FSM1/FSM2/FSM3 respectively.
//   - 6.20.2: the FSM1 definition (looked up by getDefName()) declares 11
//     Parameters (ST_Read..ST_Done), each with an explicit "[3:0]" range
//     (carried by the Parameter's typespec) and a decimal Constant value
//     (ParamAssign rhs); ST_Read=0 and ST_Done=10 spot-checked.
//   - 9.4.2: FSM1 has exactly 3 processes: 2 Always (SEQ posedge Clk,
//     COMB level-sensitive on CurState alone -- a single-signal
//     sensitivity list, so the EventControl condition is a bare RefObj,
//     not an Operation) and 1 Initial.
//   - SEQ: EventControl(vpiPosedgeOp, RefObj "Clk"); body (Begin, name
//     "SEQ") is directly an IfElse "if (Reset) CurState = ST_Read; else
//     CurState = NextState;" -- both branches are bare (non-delayed)
//     blocking Assignments, not wrapped in a Begin (single statement
//     each).
//   - COMB: EventControl condition is directly RefObj "CurState" (a
//     1-signal sensitivity list has no "or"/Operation wrapper); body
//     (Begin, name "COMB") is directly a CaseStmt with 12 CaseItems
//     (11 named states + default).
//   - The "ST_Read" CaseItem (12.5.1/Annex 37.72): exprs = [RefObj
//     "ST_Read"]; stmt is a Begin of 5 blocking Assignments ("Read=1;
//     Write=0; Wait=~Wait; Delay=~Delay; NextState=ST_Write;"); the
//     first ("Read=1") and last ("NextState=ST_Write", rhs RefObj
//     resolving to the ST_Write Parameter) are checked directly.
//
// What is NOT checked and why:
//   - FSM2/FSM3's own SEQ/COMB structure -- FSM2 mirrors FSM1's shape
//     (with an added "task SwitchCtrl" call inside case items) and FSM3
//     is a simpler negedge-sensitive FSM; per the "representative
//     subset" guidance only FSM1 is walked in structural depth.
//   - The remaining 10 non-default CaseItems of FSM1 -- same shape as
//     ST_Read, already verified once.
//   - Runtime simulation behavior (the "initial"/"forever"/"fork...join"
//     stimulus in "top") -- HLC is an elaborator with no simulator.

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
#include <hldb/design.h>
#include <hldb/event_control.h>
#include <hldb/if_else.h>
#include <hldb/initial.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class FSMBsp13Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "FSMBsp13.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }

  // The design is not elaborated; FSM1 is a module definition in
  // getAllModules(), looked up by its definition name.
  static const hldb::Module *getFSM1Def() {
    return hldb::findByDefName<hldb::Module>("FSM1", m_design->getAllModules());
  }

  static const hldb::Parameter *getFSM1Parameter(std::string_view name) {
    const hldb::Module *const fsm1 = getFSM1Def();
    if (fsm1 == nullptr) return nullptr;
    return hldb::findByName<hldb::Parameter>(name, fsm1->getParameters());
  }

  static const hldb::Begin *getFSM1NamedAlwaysBody(std::string_view label) {
    const hldb::Module *const fsm1 = getFSM1Def();
    if (fsm1 == nullptr || fsm1->getProcesses() == nullptr) return nullptr;
    for (const hldb::Process *const process : *fsm1->getProcesses()) {
      const hldb::Always *const always = any_cast<hldb::Always>(process);
      if (always == nullptr) continue;
      const hldb::EventControl *const ec = always->getStmt<hldb::EventControl>();
      if (ec == nullptr) continue;
      const hldb::Begin *const body = ec->getStmt<hldb::Begin>();
      if ((body != nullptr) && (body->getName() == label)) return body;
    }
    return nullptr;
  }

  static const hldb::EventControl *getFSM1EventControlFor(std::string_view label) {
    const hldb::Module *const fsm1 = getFSM1Def();
    if (fsm1 == nullptr || fsm1->getProcesses() == nullptr) return nullptr;
    for (const hldb::Process *const process : *fsm1->getProcesses()) {
      const hldb::Always *const always = any_cast<hldb::Always>(process);
      if (always == nullptr) continue;
      const hldb::EventControl *const ec = always->getStmt<hldb::EventControl>();
      if (ec == nullptr) continue;
      const hldb::Begin *const body = ec->getStmt<hldb::Begin>();
      if ((body != nullptr) && (body->getName() == label)) return ec;
    }
    return nullptr;
  }

  static const hldb::CaseStmt *getFSM1CombCaseStmt() {
    const hldb::Begin *const body = getFSM1NamedAlwaysBody("COMB");
    if (body == nullptr || body->getStmts() == nullptr || body->getStmts()->empty()) return nullptr;
    return any_cast<hldb::CaseStmt>(body->getStmts()->at(0));
  }
};

// --- top: instance hierarchy ----------------------------------------------

TEST_F(FSMBsp13Test, TopModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(FSMBsp13Test, TopHasThreeModuleInstances) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  // ASSERT_NE(top->getModules(), nullptr);
  // EXPECT_EQ(top->getModules()->size(), 3u);
  ASSERT_NE(top->getRefInstances(), nullptr);
  EXPECT_EQ(top->getRefInstances()->size(), 3u);

  const std::pair<std::string_view, std::string_view> expected[3] = {
      {"F1", "FSM1"},
      {"F2", "FSM2"},
      {"F3", "FSM3"},
  };
  for (const std::pair<std::string_view, std::string_view> &entry : expected) {
    // const hldb::Module *const inst = hldb::findByName<hldb::Module>(entry.first, top->getModules());
    // ASSERT_NE(inst, nullptr) << entry.first;
    // EXPECT_EQ(inst->getDefName(), entry.second) << entry.first;
    const hldb::RefInstance *const inst = hldb::findByName<hldb::RefInstance>(entry.first, top->getRefInstances());
    ASSERT_NE(inst, nullptr) << entry.first;
    ASSERT_NE(inst->getTypespec(), nullptr) << entry.first;
    const hldb::ModuleTypespec *const mt = inst->getTypespec()->getActual<hldb::ModuleTypespec>();
    ASSERT_NE(mt, nullptr) << entry.first;
    EXPECT_EQ(mt->getName(), entry.second) << entry.first;
  }
}

// --- FSM1: parameters -----------------------------------------------------

TEST_F(FSMBsp13Test, FSM1DefinitionExists) { EXPECT_NE(getFSM1Def(), nullptr); }

TEST_F(FSMBsp13Test, FSM1HasElevenRangedParameters) {
  const hldb::Module *const fsm1 = getFSM1Def();
  ASSERT_NE(fsm1, nullptr);
  ASSERT_NE(fsm1->getParamAssigns(), nullptr);
  EXPECT_EQ(fsm1->getParamAssigns()->size(), 11u);

  for (std::string_view name : {"ST_Read", "ST_Write", "ST_Delay", "ST_Trx", "ST_Hold", "ST_Block", "ST_Wait",
                                "ST_Turn", "ST_Quit", "ST_Exit", "ST_Done"}) {
    const hldb::Parameter *const param = getFSM1Parameter(name);
    ASSERT_NE(param, nullptr) << name;
    // EXPECT_NE(param->getRanges(), nullptr) << name << ": 'parameter [3:0]' declares an explicit range";
    ASSERT_NE(param->getTypespec(), nullptr) << name;
    const hldb::LogicTypespec *const ts = param->getTypespec()->getActual<hldb::LogicTypespec>();
    ASSERT_NE(ts, nullptr) << name << ": 'parameter [3:0]' is an implicit logic vector";
    EXPECT_NE(ts->getRanges(), nullptr) << name << ": 'parameter [3:0]' declares an explicit range";
  }
}

TEST_F(FSMBsp13Test, FSM1StReadIsZeroStDoneIsTen) {
  const hldb::Module *const fsm1 = getFSM1Def();
  ASSERT_NE(fsm1, nullptr);
  ASSERT_NE(fsm1->getParamAssigns(), nullptr);

  const hldb::Parameter *const stRead = getFSM1Parameter("ST_Read");
  ASSERT_NE(stRead, nullptr);
  const hldb::Parameter *const stDone = getFSM1Parameter("ST_Done");
  ASSERT_NE(stDone, nullptr);

  const hldb::Constant *readValue = nullptr;
  const hldb::Constant *doneValue = nullptr;
  for (const hldb::ParamAssign *const pa : *fsm1->getParamAssigns()) {
    // if (pa->getLhs<hldb::Parameter>() == stRead) readValue = pa->getRhs<hldb::Constant>();
    // if (pa->getLhs<hldb::Parameter>() == stDone) doneValue = pa->getRhs<hldb::Constant>();
    const hldb::RefObj *const lhs = pa->getLhs<hldb::RefObj>();
    if (lhs == nullptr) continue;
    if (lhs->getActual() == stRead) readValue = pa->getRhs<hldb::Constant>();
    if (lhs->getActual() == stDone) doneValue = pa->getRhs<hldb::Constant>();
  }
  ASSERT_NE(readValue, nullptr);
  EXPECT_EQ(readValue->getDecompile(), std::string_view{"0"});
  ASSERT_NE(doneValue, nullptr);
  EXPECT_EQ(doneValue->getDecompile(), std::string_view{"10"});
}

// --- FSM1: SEQ / COMB process pair ----------------------------------------

TEST_F(FSMBsp13Test, FSM1HasExactlyTwoAlwaysProcesses) {
  const hldb::Module *const fsm1 = getFSM1Def();
  ASSERT_NE(fsm1, nullptr);
  ASSERT_NE(fsm1->getProcesses(), nullptr);
  // EXPECT_EQ(fsm1->getProcesses()->size(), 2u);
  EXPECT_EQ(fsm1->getProcesses()->size(), 3u) << "SEQ always, initial, COMB always";
  uint32_t alwaysCount = 0;
  uint32_t initialCount = 0;
  for (const hldb::Process *const process : *fsm1->getProcesses()) {
    if (any_cast<hldb::Initial>(process) != nullptr) {
      ++initialCount;
      continue;
    }
    const hldb::Always *const always = any_cast<hldb::Always>(process);
    ASSERT_NE(always, nullptr);
    EXPECT_EQ(always->getAlwaysType(), vpiAlways);
    ++alwaysCount;
  }
  EXPECT_EQ(alwaysCount, 2u);
  EXPECT_EQ(initialCount, 1u);
}

TEST_F(FSMBsp13Test, SeqIsPosedgeClkWithBareIfElse) {
  const hldb::EventControl *const ec = getFSM1EventControlFor("SEQ");
  ASSERT_NE(ec, nullptr) << "FSM1's SEQ block not found";
  ASSERT_NE(ec->getCondition(), nullptr);
  const hldb::Operation *const cond = ec->getCondition<hldb::Operation>();
  ASSERT_NE(cond, nullptr);
  EXPECT_EQ(cond->getOpType(), vpiPosedgeOp);
  ASSERT_NE(cond->getOperands(), nullptr);
  ASSERT_EQ(cond->getOperands()->size(), 1u);
  const hldb::RefObj *const operand = any_cast<hldb::RefObj>(cond->getOperands()->front());
  ASSERT_NE(operand, nullptr);
  EXPECT_EQ(operand->getName(), std::string_view{"Clk"});

  const hldb::Begin *const body = getFSM1NamedAlwaysBody("SEQ");
  ASSERT_NE(body, nullptr);
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 1u);
  const hldb::IfElse *const ifElse = any_cast<hldb::IfElse>(body->getStmts()->at(0));
  ASSERT_NE(ifElse, nullptr) << "'if (Reset) CurState = ST_Read; else CurState = NextState;'";

  const hldb::Assignment *const thenAssign = ifElse->getStmt<hldb::Assignment>();
  ASSERT_NE(thenAssign, nullptr) << "then-branch (single stmt) must not be wrapped in a Begin";
  // EXPECT_FALSE(thenAssign->getBlocking());
  EXPECT_TRUE(thenAssign->getBlocking()) << "'CurState = ST_Read;' is a blocking assignment";
  EXPECT_EQ(thenAssign->getDelayControl(), nullptr) << "'CurState = ST_Read;' has no intra-assignment delay";

  const hldb::Assignment *const elseAssign = ifElse->getElseStmt<hldb::Assignment>();
  ASSERT_NE(elseAssign, nullptr) << "else-branch (single stmt) must not be wrapped in a Begin";
  // EXPECT_FALSE(elseAssign->getBlocking());
  EXPECT_TRUE(elseAssign->getBlocking()) << "'CurState = NextState;' is a blocking assignment";
  const hldb::RefObj *const elseRhs = elseAssign->getRhs<hldb::RefObj>();
  ASSERT_NE(elseRhs, nullptr);
  EXPECT_EQ(elseRhs->getName(), std::string_view{"NextState"});
}

TEST_F(FSMBsp13Test, CombSensitivityListIsBareRefObjOnCurState) {
  const hldb::EventControl *const ec = getFSM1EventControlFor("COMB");
  ASSERT_NE(ec, nullptr) << "FSM1's COMB block not found";
  ASSERT_NE(ec->getCondition(), nullptr);
  const hldb::RefObj *const cond = ec->getCondition<hldb::RefObj>();
  ASSERT_NE(cond, nullptr) << "a single-signal sensitivity list ('@(CurState)') should be a bare RefObj, "
                              "not wrapped in an Operation";
  EXPECT_EQ(cond->getName(), std::string_view{"CurState"});
}

TEST_F(FSMBsp13Test, CombBodyIsDirectlyCaseStmtWithTwelveItems) {
  const hldb::CaseStmt *const cs = getFSM1CombCaseStmt();
  ASSERT_NE(cs, nullptr) << "'case (CurState) ... endcase', no begin/end around it, must not be wrapped in a Begin";
  EXPECT_EQ(cs->getCaseType(), vpiCaseExact);
  ASSERT_NE(cs->getCondition(), nullptr);
  const hldb::RefObj *const cond = cs->getCondition<hldb::RefObj>();
  ASSERT_NE(cond, nullptr);
  EXPECT_EQ(cond->getName(), std::string_view{"CurState"});
  ASSERT_NE(cs->getCaseItems(), nullptr);
  EXPECT_EQ(cs->getCaseItems()->size(), 12u) << "11 named states + default";
}

TEST_F(FSMBsp13Test, StReadCaseItemHasFiveBlockingAssignsEndingInNextStateWrite) {
  const hldb::CaseStmt *const cs = getFSM1CombCaseStmt();
  ASSERT_NE(cs, nullptr);
  ASSERT_NE(cs->getCaseItems(), nullptr);

  const hldb::CaseItem *stReadItem = nullptr;
  for (const hldb::CaseItem *const item : *cs->getCaseItems()) {
    if (item == nullptr || item->getExprs() == nullptr || item->getExprs()->empty()) continue;
    const hldb::RefObj *const label = any_cast<hldb::RefObj>(item->getExprs()->at(0));
    if ((label != nullptr) && (label->getName() == "ST_Read")) {
      stReadItem = item;
      break;
    }
  }
  ASSERT_NE(stReadItem, nullptr) << "'ST_Read:' case item not found";

  const hldb::Begin *const body = any_cast<hldb::Begin>(stReadItem->getStmt());
  ASSERT_NE(body, nullptr);
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 5u);

  const hldb::Assignment *const readAssign = any_cast<hldb::Assignment>(body->getStmts()->at(0));
  ASSERT_NE(readAssign, nullptr) << "'Read = 1;'";
  EXPECT_TRUE(readAssign->getBlocking());
  const hldb::RefObj *const readLhs = readAssign->getLhs<hldb::RefObj>();
  ASSERT_NE(readLhs, nullptr);
  EXPECT_EQ(readLhs->getName(), std::string_view{"Read"});
  ASSERT_NE(readAssign->getRhs(), nullptr);
  const hldb::Constant *const readRhs = readAssign->getRhs<hldb::Constant>();
  ASSERT_NE(readRhs, nullptr);
  EXPECT_EQ(readRhs->getDecompile(), std::string_view{"1"});

  const hldb::Assignment *const nextStateAssign = any_cast<hldb::Assignment>(body->getStmts()->at(4));
  ASSERT_NE(nextStateAssign, nullptr) << "'NextState = ST_Write;'";
  EXPECT_TRUE(nextStateAssign->getBlocking());
  const hldb::RefObj *const nsLhs = nextStateAssign->getLhs<hldb::RefObj>();
  ASSERT_NE(nsLhs, nullptr);
  EXPECT_EQ(nsLhs->getName(), std::string_view{"NextState"});
  ASSERT_NE(nextStateAssign->getRhs(), nullptr);
  const hldb::RefObj *const nsRhs = nextStateAssign->getRhs<hldb::RefObj>();
  ASSERT_NE(nsRhs, nullptr);
  EXPECT_EQ(nsRhs->getName(), std::string_view{"ST_Write"});
  EXPECT_EQ(nsRhs->getActual(), getFSM1Parameter("ST_Write"));
}

// --- compiler diagnostics -------------------------------------------------

TEST_F(FSMBsp13Test, NoFailedBindsInFSM1) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "CurState"), nullptr)
      << "'CurState' references inside FSM1 must bind";
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "NextState"), nullptr)
      << "'NextState' references inside FSM1 must bind";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
