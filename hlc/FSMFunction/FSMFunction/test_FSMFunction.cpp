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

// Tests for tests/FSMFunction/top.sv (module fsm_using_function)
//
//   module fsm_using_function (clock, reset, req_0, req_1, gnt_0, gnt_1);
//     ...
//     reg   [SIZE-1:0] state;
//     wire  [SIZE-1:0] next_state;
//     assign next_state = fsm_function(state, req_0, req_1);
//     function [SIZE-1:0] fsm_function;
//       input [SIZE-1:0] state;
//       input req_0;
//       input req_1;
//       case(state)
//         IDLE : if (req_0==1'b1) fsm_function = GNT0; else if (...) ...
//         ...
//         default : fsm_function = IDLE;
//       endcase
//     endfunction
//     always @ (posedge clock) begin : FSM_SEQ ... end
//     always @ (posedge clock) begin : OUTPUT_LOGIC ... end
//   endmodule
//
// This is the "FSM combo logic implemented via a function call" style:
// instead of a combinational always block, next_state is driven by a
// continuous assignment that calls a function; the function itself
// contains the same case/if-else next-state logic seen in FSM2Always and
// FSMSingleAlways, just wrapped in a function body with its own IODecls.
//
// What is checked (IEEE 1800-2023 citations):
//   - 6.7/6.8: "wire next_state" (net-type keyword, no separate
//     declaration assignment: it is continuously driven, not
//     initialized) -> Net, not Variable; "reg state" -> Variable.
//   - 13.4 "Functions": the module has exactly 1 Function
//     ("fsm_function") among its getTaskFuncs(); it declares 3 IODecls
//     (state, req_0, req_1), each vpiInput (13.4/Table 13-1 default
//     direction for a function's formal arguments); its body statement
//     (no begin/end after the input declarations) is directly a
//     CaseStmt, not wrapped in a Begin.
//   - 10.3.2 "Continuous assignment": "assign next_state =
//     fsm_function(state, req_0, req_1);" produces exactly 1 ContAssign
//     on the module, lhs RefObj "next_state", rhs a FuncCall named
//     "fsm_function" whose getTaskFunc() resolves back to the Function
//     object, and whose 3 arguments are RefObj state/req_0/req_1 in
//     declared order.
//   - Inside the function: the CaseStmt condition is RefObj "state" (13.4:
//     this resolves to the function's own IODecl "state", not the
//     module-scope Variable "state" of the same name -- a shadowing
//     Any distinct from the module-level Variable); 4 CaseItems (IDLE,
//     GNT0, GNT1, default); the default item assigns
//     "fsm_function = IDLE" (13.4.3: a plain assignment to the function
//     name inside its own body sets the implicit return value), a
//     blocking Assignment whose rhs is RefObj "IDLE" resolving to the
//     module-level Parameter.
//
// What is NOT checked and why:
//   - The IDLE/GNT0/GNT1 case items' full if-else-chain bodies -- the
//     same IfElse/Assignment shapes are already verified structurally in
//     FSM2Always's ComboIdleCaseItemBodyIsIfElseChain and
//     FSMSingleAlways's IdleCaseItemMixesDelayedAndUndelayedNonBlocking-
//     Assigns; only the default item and the case's condition/shape are
//     re-verified here, per the "representative subset" guidance.
//   - Runtime FSM transition behavior -- HLC is an elaborator with no
//     simulator.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/case_item.h>
#include <hldb/case_stmt.h>
#include <hldb/constant.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/func_call.h>
#include <hldb/function.h>
#include <hldb/io_decl.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/ref_obj.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class FSMFunctionTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "FSMFunction.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getModule() {
    return hldb::findByName<hldb::Module>("fsm_using_function", m_design->getAllModules());
  }

  static const hldb::Parameter *getParameter(std::string_view name) {
    const hldb::Module *const mod = getModule();
    if (mod == nullptr) return nullptr;
    return hldb::findByName<hldb::Parameter>(name, mod->getParameters());
  }

  static const hldb::Function *getFunction() {
    const hldb::Module *const mod = getModule();
    if (mod == nullptr) return nullptr;
    return hldb::findByName<hldb::Function>("fsm_function", mod->getTaskFuncs());
  }

  static const hldb::ContAssign *getNextStateContAssign() {
    const hldb::Module *const mod = getModule();
    if (mod == nullptr || mod->getContAssigns() == nullptr) return nullptr;
    for (const hldb::ContAssign *const ca : *mod->getContAssigns()) {
      if (ca == nullptr || ca->getLhs() == nullptr) continue;
      const hldb::RefObj *const lhs = ca->getLhs<hldb::RefObj>();
      if ((lhs != nullptr) && (lhs->getName() == "next_state")) return ca;
    }
    return nullptr;
  }

  static const hldb::CaseStmt *getFunctionCaseStmt() {
    const hldb::Function *const fn = getFunction();
    if (fn == nullptr) return nullptr;
    return fn->getStmt<hldb::CaseStmt>();
  }
};

// --- module / declarations ----------------------------------------------

TEST_F(FSMFunctionTest, ModuleExists) { EXPECT_NE(getModule(), nullptr); }

TEST_F(FSMFunctionTest, NextStateIsNetStateIsVariable) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  const hldb::Net *const nextState = hldb::findByName<hldb::Net>("next_state", mod->getNets());
  ASSERT_NE(nextState, nullptr) << "'wire [SIZE-1:0] next_state' has a net-type keyword (6.7)";
  EXPECT_EQ(hldb::findByName<hldb::Variable>("next_state", mod->getVariables()), nullptr);

  const hldb::Variable *const state = hldb::findByName<hldb::Variable>("state", mod->getVariables());
  ASSERT_NE(state, nullptr) << "'reg [SIZE-1:0] state' has no net-type keyword (6.8)";
  EXPECT_EQ(hldb::findByName<hldb::Net>("state", mod->getNets()), nullptr);
}

TEST_F(FSMFunctionTest, FourParametersExist) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getParamAssigns(), nullptr);
  EXPECT_EQ(mod->getParamAssigns()->size(), 4u);
  for (std::string_view name : {"SIZE", "IDLE", "GNT0", "GNT1"}) {
    EXPECT_NE(getParameter(name), nullptr) << name;
  }
}

// --- function fsm_function -------------------------------------------------

TEST_F(FSMFunctionTest, ModuleHasExactlyOneFunction) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getTaskFuncs(), nullptr);
  EXPECT_EQ(mod->getTaskFuncs()->size(), 1u);
  EXPECT_NE(getFunction(), nullptr);
}

TEST_F(FSMFunctionTest, FunctionHasThreeInputIODecls) {
  const hldb::Function *const fn = getFunction();
  ASSERT_NE(fn, nullptr);
  ASSERT_NE(fn->getIODecls(), nullptr);
  ASSERT_EQ(fn->getIODecls()->size(), 3u);
  const std::string_view expectedNames[3] = {"state", "req_0", "req_1"};
  size_t index = 0;
  for (const hldb::IODecl *const io : *fn->getIODecls()) {
    ASSERT_NE(io, nullptr) << index;
    EXPECT_EQ(io->getName(), expectedNames[index]) << index;
    EXPECT_EQ(io->getDirection(), vpiInput) << "13.4: a function's formal arguments default to input";
    ++index;
  }
}

TEST_F(FSMFunctionTest, FunctionBodyIsDirectlyCaseStmt) {
  const hldb::CaseStmt *const cs = getFunctionCaseStmt();
  EXPECT_NE(cs, nullptr) << "'case(state) ... endcase' after the input decls, no begin/end, "
                            "must not be wrapped in a Begin";
}

TEST_F(FSMFunctionTest, CaseConditionResolvesToStateWithFourItems) {
  const hldb::CaseStmt *const cs = getFunctionCaseStmt();
  ASSERT_NE(cs, nullptr);
  EXPECT_EQ(cs->getCaseType(), vpiCaseExact);
  ASSERT_NE(cs->getCondition(), nullptr);
  const hldb::RefObj *const cond = cs->getCondition<hldb::RefObj>();
  ASSERT_NE(cond, nullptr);
  EXPECT_EQ(cond->getName(), std::string_view{"state"});
  ASSERT_NE(cond->getActual(), nullptr) << "'state' inside the function should bind to something in scope";

  ASSERT_NE(cs->getCaseItems(), nullptr);
  EXPECT_EQ(cs->getCaseItems()->size(), 4u) << "IDLE, GNT0, GNT1, default";
}

TEST_F(FSMFunctionTest, DefaultCaseItemAssignsFunctionNameToIdle) {
  const hldb::CaseStmt *const cs = getFunctionCaseStmt();
  ASSERT_NE(cs, nullptr);
  ASSERT_NE(cs->getCaseItems(), nullptr);
  ASSERT_GE(cs->getCaseItems()->size(), 4u);
  const hldb::CaseItem *const defaultItem = cs->getCaseItems()->at(3);
  ASSERT_NE(defaultItem, nullptr);
  EXPECT_EQ(defaultItem->getExprs(), nullptr) << "default case item must have no case_item_expression";

  const hldb::Assignment *const assign = defaultItem->getStmt<hldb::Assignment>();
  ASSERT_NE(assign, nullptr) << "'default : fsm_function = IDLE;'";
  EXPECT_TRUE(assign->getBlocking());
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), std::string_view{"fsm_function"})
      << "13.4.3: assigning to the function's own name sets its implicit return value";
  ASSERT_NE(assign->getRhs(), nullptr);
  const hldb::RefObj *const rhs = assign->getRhs<hldb::RefObj>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getName(), std::string_view{"IDLE"});
  EXPECT_EQ(rhs->getActual(), getParameter("IDLE"));
}

// --- assign next_state = fsm_function(state, req_0, req_1); --------------

TEST_F(FSMFunctionTest, ExactlyOneContAssignDrivingNextState) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getContAssigns(), nullptr);
  EXPECT_EQ(mod->getContAssigns()->size(), 1u);
  EXPECT_NE(getNextStateContAssign(), nullptr);
}

TEST_F(FSMFunctionTest, ContAssignRhsIsFuncCallResolvingToFsmFunction) {
  const hldb::ContAssign *const ca = getNextStateContAssign();
  ASSERT_NE(ca, nullptr);
  ASSERT_NE(ca->getRhs(), nullptr);
  const hldb::FuncCall *const call = ca->getRhs<hldb::FuncCall>();
  ASSERT_NE(call, nullptr) << "'fsm_function(state, req_0, req_1)' must be a FuncCall";
  EXPECT_EQ(call->getName(), std::string_view{"fsm_function"});
  EXPECT_EQ(call->getTaskFunc(), getFunction());
}

TEST_F(FSMFunctionTest, FuncCallHasThreeArgumentsInDeclaredOrder) {
  const hldb::ContAssign *const ca = getNextStateContAssign();
  ASSERT_NE(ca, nullptr);
  const hldb::FuncCall *const call = ca->getRhs<hldb::FuncCall>();
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 3u);
  const std::string_view expectedNames[3] = {"state", "req_0", "req_1"};
  size_t index = 0;
  for (const hldb::Any *const arg : *call->getArguments()) {
    ASSERT_NE(arg, nullptr) << index;
    const hldb::RefObj *const ref = any_cast<hldb::RefObj>(arg);
    ASSERT_NE(ref, nullptr) << index;
    EXPECT_EQ(ref->getName(), expectedNames[index]) << index;
    ++index;
  }
}

// --- compiler diagnostics -------------------------------------------------

TEST_F(FSMFunctionTest, NoFailedBinds) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND), nullptr)
      << "all references (state/req_0/req_1/parameters/fsm_function) must bind";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
