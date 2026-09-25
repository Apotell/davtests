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

// Tests for FuncBinding.hlc (tests/FuncBinding/dut.sv):
//
//   module fsm_using_function (
//     input bit clock, input bit reset,
//     input bit req_0, input bit req_1,
//     output bit gnt_0, output bit gnt_1);
//
//     typedef enum logic [1:0] {IDLE = 1, GNT0 = 2, GNT1 = 3} state;
//     state curr_state, next_state;
//
//     always @(posedge clock)
//     begin : FUN
//       assign next_state = fsm_function(curr_state, req_0, req_1, next_state);
//     end
//
//     function fsm_function(input logic [1:0] state, input bit req_0,
//                            input bit req_1, output logic [1:0] future_state);
//       case (state)
//         IDLE: if (req_0) future_state = GNT0; else if (req_1) future_state = GNT1;
//               else future_state = IDLE;
//         GNT0: if (req_0) future_state = GNT0; else future_state = IDLE;
//         GNT1: if (req_1) future_state = GNT1; else future_state = IDLE;
//         default: future_state = IDLE;
//       endcase
//     endfunction
//
//     always @(posedge clock) begin : FSM_SEQ ... end
//   endmodule
//
// What is under test: ordinary (non-generate) binding of a function call to
// a same-module function declaration, where the call textually *precedes*
// the declaration in the source (Sec 23.9 "Scope rules": declaration order
// within a module scope does not matter for name resolution -- all
// declarations in a scope are visible throughout that scope). The call
// itself sits inside a procedural 'assign' statement (Sec 10.6 "Procedural
// continuous assignments") nested inside a labeled 'begin : FUN ... end'
// block within an 'always' process.
//
// No .log file was consulted; accessor names were confirmed against the
// real hldb headers under
// E:\Davenche\hlc\hlc_03\out\install\x64-Debug\include\hldb.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/always.h>
#include <hldb/assign_stmt.h>
#include <hldb/begin.h>
#include <hldb/case_item.h>
#include <hldb/case_stmt.h>
#include <hldb/design.h>
#include <hldb/event_control.h>
#include <hldb/func_call.h>
#include <hldb/function.h>
#include <hldb/io_decl.h>
#include <hldb/module.h>
#include <hldb/ref_obj.h>
#include <hldb/vpi_user.h>

namespace hlc {

class FuncBindingTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "FuncBinding.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() {
    return hldb::findByName<hldb::Module>("fsm_using_function", m_design->getAllModules());
  }

  static const hldb::Function *getFsmFunction(const hldb::Module *m) {
    return (m == nullptr) ? nullptr : hldb::findByName<hldb::Function>("fsm_function", m->getTaskFuncs());
  }

  // Finds the Always block whose EventControl-guarded Begin carries the
  // given end label.
  static const hldb::Begin *findAlwaysBeginByLabel(const hldb::Module *m, std::string_view label) {
    if (m == nullptr || m->getProcesses() == nullptr) return nullptr;
    for (const hldb::Process *const p : *m->getProcesses()) {
      const hldb::Always *const alw = any_cast<hldb::Always>(p);
      if (alw == nullptr) continue;
      const hldb::EventControl *const ec = alw->getStmt<hldb::EventControl>();
      if (ec == nullptr) continue;
      const hldb::Begin *const blk = ec->getStmt<hldb::Begin>();
      if (blk != nullptr && blk->getEndLabel() == label) return blk;
    }
    return nullptr;
  }
};

TEST_F(FuncBindingTest, ModuleExists) { ASSERT_NE(getTop(), nullptr); }

TEST_F(FuncBindingTest, FsmFunctionDeclaredWithFourIODecls) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Function *const fn = getFsmFunction(top);
  ASSERT_NE(fn, nullptr) << "'fsm_function' not found among module task/functions";

  ASSERT_NE(fn->getIODecls(), nullptr);
  ASSERT_EQ(fn->getIODecls()->size(), 4u);

  const hldb::IODecl *pState = nullptr;
  const hldb::IODecl *pReq0 = nullptr;
  const hldb::IODecl *pReq1 = nullptr;
  const hldb::IODecl *pFuture = nullptr;
  for (const hldb::IODecl *const io : *fn->getIODecls()) {
    ASSERT_NE(io, nullptr);
    if (io->getName() == "state") pState = io;
    if (io->getName() == "req_0") pReq0 = io;
    if (io->getName() == "req_1") pReq1 = io;
    if (io->getName() == "future_state") pFuture = io;
  }
  ASSERT_NE(pState, nullptr);
  ASSERT_NE(pReq0, nullptr);
  ASSERT_NE(pReq1, nullptr);
  ASSERT_NE(pFuture, nullptr);
  EXPECT_EQ(pState->getDirection(), vpiInput);
  EXPECT_EQ(pReq0->getDirection(), vpiInput);
  EXPECT_EQ(pReq1->getDirection(), vpiInput);
  EXPECT_EQ(pFuture->getDirection(), vpiOutput)
      << "'output logic [1:0] future_state' must carry direction vpiOutput (Sec 13.4)";
}

// 'case (state) ... endcase' -- IEEE 1800-2023 12.5, a plain (non-x/z,
// non-unique/priority) case statement: getCaseType() == vpiCaseExact,
// getQualifier() == vpiNoQualifier, and 4 case items (IDLE, GNT0, GNT1,
// default).
TEST_F(FuncBindingTest, FsmFunctionBodyIsPlainCaseOnStateWithFourItems) {
  const hldb::Function *const fn = getFsmFunction(getTop());
  ASSERT_NE(fn, nullptr);
  const hldb::CaseStmt *const cs = fn->getStmt<hldb::CaseStmt>();
  ASSERT_NE(cs, nullptr) << "function body should be a single CaseStmt (no begin/end wraps it)";
  EXPECT_EQ(cs->getCaseType(), vpiCaseExact);
  EXPECT_EQ(cs->getQualifier(), vpiNoQualifier);

  const hldb::RefObj *const cond = cs->getCondition<hldb::RefObj>();
  ASSERT_NE(cond, nullptr) << "'case (state)' condition should be a RefObj";
  EXPECT_EQ(cond->getName(), std::string_view("state"));

  ASSERT_NE(cs->getCaseItems(), nullptr);
  EXPECT_EQ(cs->getCaseItems()->size(), 4u) << "IDLE, GNT0, GNT1, default";

  bool sawDefault = false;
  for (const hldb::CaseItem *const item : *cs->getCaseItems()) {
    ASSERT_NE(item, nullptr);
    if (item->getExprs() == nullptr || item->getExprs()->empty()) sawDefault = true;
  }
  EXPECT_TRUE(sawDefault) << "Sec 12.5: the 'default' case item has no case_item_expression list";
}

// Two 'always @(posedge clock)' processes: 'FUN' (procedural assign calling
// fsm_function) and 'FSM_SEQ' (state register).
TEST_F(FuncBindingTest, ModuleHasTwoAlwaysBlocksFunAndFsmSeq) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_NE(findAlwaysBeginByLabel(top, "FUN"), nullptr) << "'begin : FUN ... end' not found";
  EXPECT_NE(findAlwaysBeginByLabel(top, "FSM_SEQ"), nullptr) << "'begin : FSM_SEQ ... end' not found";
}

// The core binding under test: 'assign next_state = fsm_function(curr_state,
// req_0, req_1, next_state);' inside 'begin : FUN' must resolve its call to
// the 'fsm_function' declared later in the same module (Sec 23.9: order of
// declaration within a scope does not affect visibility within that scope).
TEST_F(FuncBindingTest, FunBlockCallsFsmFunctionBoundToItsDeclaration) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Function *const fn = getFsmFunction(top);
  ASSERT_NE(fn, nullptr);

  const hldb::Begin *const funBlk = findAlwaysBeginByLabel(top, "FUN");
  ASSERT_NE(funBlk, nullptr);
  ASSERT_NE(funBlk->getStmts(), nullptr);
  ASSERT_EQ(funBlk->getStmts()->size(), 1u);
  const hldb::AssignStmt *const asg = any_cast<hldb::AssignStmt>(funBlk->getStmts()->at(0));
  ASSERT_NE(asg, nullptr) << "'assign next_state = ...;' inside a procedural block is a procedural "
                              "continuous assignment (AssignStmt, Sec 10.6.2)";

  const hldb::RefObj *const lhs = asg->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), std::string_view("next_state"));

  const hldb::FuncCall *const call = asg->getRhs<hldb::FuncCall>();
  ASSERT_NE(call, nullptr) << "RHS should be a FuncCall to 'fsm_function'";
  EXPECT_EQ(call->getName(), std::string_view("fsm_function"));
  EXPECT_EQ(call->getTaskFunc<hldb::Function>(), fn)
      << "the call must bind back to the 'fsm_function' declaration, even though the declaration "
         "textually follows the call site";

  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 4u) << "curr_state, req_0, req_1, next_state";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
