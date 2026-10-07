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

// Tests for design.sv + testbench.sv (tags: ApbSlave), compiled together as
// "design.sv testbench.sv", with testbench.sv also importing the third-party
// "svunit_pkg" (third_party/UVM/svunit_base).
//   design.sv:
//     module apb_slave #(addrWidth = 8, dataWidth = 32) (
//       input clk, rst_n, input [addrWidth-1:0] paddr, input pwrite, psel, penable,
//       input [dataWidth-1:0] pwdata, output logic [dataWidth-1:0] prdata);
//       logic [dataWidth-1:0] mem [256];
//       logic [1:0] apb_st;
//       const logic [1:0] SETUP = 0, W_ENABLE = 1, R_ENABLE = 2;
//       always @(negedge rst_n or posedge clk) begin
//         if (rst_n == 0) begin apb_st <= 0; prdata <= 0; end
//         else case (apb_st)
//           SETUP: begin ... if (psel && !penable) begin if (pwrite) apb_st <= W_ENABLE;
//                              else apb_st <= R_ENABLE; end end
//           W_ENABLE: begin if (psel && penable && pwrite) mem[paddr] <= pwdata;
//                           apb_st <= SETUP; end
//           R_ENABLE: begin if (psel && penable && !pwrite) prdata <= mem[paddr];
//                           apb_st <= SETUP; end
//         endcase
//       end
//     endmodule
//   testbench.sv (SVUnit, not UVM classes -- plain module + task macros):
//     module apb_slave_unit_test;
//       ...
//       apb_slave my_apb_slave(.*);
//       task write(logic [7:0] addr, logic [31:0] data, logic back2back = 0,
//                  logic setup_psel = 1, logic setup_pwrite = 1); ... endtask
//       task read(...); task idle(...);
//       `SVUNIT_TESTS_BEGIN ... 4x `SVTEST(...)...`SVTEST_END ... `SVUNIT_TESTS_END
//     endmodule
//
// This file focuses on design.sv's state machine (the actual APB-slave RTL)
// and on the parts of testbench.sv that are specific to this test (the
// wildcard-connected DUT instance and the write/read/idle task signatures),
// not on the SVUnit test-body macro expansion itself (the giant generated
// "run" task), which is third-party test-framework boilerplate rather than
// anything specific to this design.
//
// Checked:
//   - "apb_slave": 2 untyped parameters (addrWidth, dataWidth -- no
//     "vpiTypespec" at all on either Parameter, since "#(addrWidth = 8, ...)"
//     gives no explicit type) defaulting to 8 and 32
//   - 8 ports in declaration order: the first 7 (clk, rst_n, paddr, pwrite,
//     psel, penable, pwdata) are input nets (vpiWire); "prdata" is an
//     output and a Variable (IEEE 1800-2023 23.2.2.3: an explicit data type
//     on an output port makes it a variable, not a net)
//   - "mem" is a 256-entry unpacked array of "logic [dataWidth-1:0]": like
//     the "[SIZE]" dimensions in the ArrayExprFuncArg test, its ArrayTypespec
//     range has a 1-operand "subtract" left expression (here over the
//     literal Constant "256", not a RefObj) and no right expression
//   - SETUP/W_ENABLE/R_ENABLE are "const logic[1:0]" variables
//     (getConstantVariable()) whose initializers fold to 0, 1, 2
//   - the always block is sensitive to "negedge rst_n or posedge clk"; its
//     body is an IfElse on "rst_n == 0" (reset clears apb_st and prdata);
//     the else branch is an exact CaseStmt on "apb_st" with exactly 3 case
//     items, in source order SETUP/W_ENABLE/R_ENABLE
//   - the W_ENABLE case item's guarded assignment is "mem[paddr] <= pwdata"
//     (a BitSelect lhs); the R_ENABLE case item's is "prdata <= mem[paddr]"
//     (a BitSelect rhs) -- the actual read/write behavior this design exists
//     to implement
//   - "apb_slave_unit_test" instantiates "apb_slave" as "my_apb_slave(.*)":
//     the RefInstance has exactly 1 Port, and that port's low connection is
//     literally a RefObj named ".*" (HLC does not expand the wildcard into
//     per-signal connections at this compile stage)
//   - task "write" has 5 IODecls in order; "addr" and "data" have no default
//     expression, while "back2back"/"setup_psel"/"setup_pwrite" default to
//     0/1/1 respectively
//   - compiling the whole design+testbench+svunit_pkg unit reports 3
//     "multiply defined typedef" errors, all located in third-party
//     svunit_base files (svunit_pkg.sv/svunit_testsuite.sv/svunit_testrunner.sv
//     all declare "svunit_testcase"/"svunit_testsuite"); neither design.sv
//     nor testbench.sv themselves fail to bind
//
// NOT CHECKED: runtime effects (the actual read/write/reset behavior of the
// APB state machine, and whether any SVTEST passes) cannot be observed --
// HLC is a compiler/elaborator with no simulation capability. The SVUnit
// macro-generated "run" task's body (svunit_pkg internals, `SVTEST` bodies)
// is also not examined here: it is third-party test-framework boilerplate,
// not part of this design.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/always.h>
#include <hldb/array_typespec.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/bit_select.h>
#include <hldb/case_item.h>
#include <hldb/case_stmt.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/event_control.h>
#include <hldb/if_else.h>
#include <hldb/if_stmt.h>
#include <hldb/io_decl.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/port.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/task.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class ApbSlaveTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "ApbSlave.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getDutModule() {
    return hldb::findByDefName<hldb::Module>("apb_slave", m_design->getAllModules());
  }

  static const hldb::Module *getTestbenchModule() {
    return hldb::findByName<hldb::Module>("apb_slave_unit_test", m_design->getAllModules());
  }

  static const hldb::Always *getAlwaysProcess() {
    const hldb::Module *const mod = getDutModule();
    if (mod == nullptr || mod->getProcesses() == nullptr || mod->getProcesses()->empty()) {
      return nullptr;
    }
    return any_cast<hldb::Always>(mod->getProcesses()->at(0));
  }

  static const hldb::IfElse *getResetIfElse() {
    const hldb::Always *const always = getAlwaysProcess();
    if (always == nullptr) {
      return nullptr;
    }
    const hldb::EventControl *const ec = always->getStmt<hldb::EventControl>();
    if (ec == nullptr) {
      return nullptr;
    }
    const hldb::Begin *const begin = ec->getStmt<hldb::Begin>();
    if (begin == nullptr || begin->getStmts() == nullptr || begin->getStmts()->empty()) {
      return nullptr;
    }
    return any_cast<hldb::IfElse>(begin->getStmts()->at(0));
  }

  static const hldb::CaseStmt *getApbStCaseStmt() {
    const hldb::IfElse *const ifElse = getResetIfElse();
    if (ifElse == nullptr) {
      return nullptr;
    }
    const hldb::Begin *const elseBody = any_cast<hldb::Begin>(ifElse->getElseStmt());
    if (elseBody == nullptr || elseBody->getStmts() == nullptr || elseBody->getStmts()->empty()) {
      return nullptr;
    }
    return any_cast<hldb::CaseStmt>(elseBody->getStmts()->at(0));
  }
};

// --- apb_slave: parameters / ports ------------------------------------------

TEST_F(ApbSlaveTest, DutModuleExists) { EXPECT_NE(getDutModule(), nullptr); }

TEST_F(ApbSlaveTest, ParametersAreUntypedAndDefaultToEightAndThirtyTwo) {
  const hldb::Module *const mod = getDutModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getParameters(), nullptr);
  ASSERT_EQ(mod->getParameters()->size(), 2u);

  const hldb::Parameter *const addrWidth = hldb::findByName<hldb::Parameter>("addrWidth", mod->getParameters());
  ASSERT_NE(addrWidth, nullptr);
  EXPECT_EQ(addrWidth->getTypespec(), nullptr) << "'addrWidth = 8' declares no explicit parameter type";

  ASSERT_NE(mod->getParamAssigns(), nullptr);
  const hldb::ParamAssign *const addrWidthAssign =
      hldb::findByName<hldb::ParamAssign, hldb::ParamAssign>("addrWidth", mod->getParamAssigns());
  ASSERT_NE(addrWidthAssign, nullptr);
  const hldb::Constant *const addrWidthVal = addrWidthAssign->getRhs<hldb::Constant>();
  ASSERT_NE(addrWidthVal, nullptr);
  EXPECT_EQ(addrWidthVal->getDecompile(), "8");

  const hldb::ParamAssign *const dataWidthAssign =
      hldb::findByName<hldb::ParamAssign, hldb::ParamAssign>("dataWidth", mod->getParamAssigns());
  ASSERT_NE(dataWidthAssign, nullptr);
  const hldb::Constant *const dataWidthVal = dataWidthAssign->getRhs<hldb::Constant>();
  ASSERT_NE(dataWidthVal, nullptr);
  EXPECT_EQ(dataWidthVal->getDecompile(), "32");
}

TEST_F(ApbSlaveTest, SevenInputNetsThenOutputVariablePort) {
  const hldb::Module *const mod = getDutModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getPorts(), nullptr);
  ASSERT_EQ(mod->getPorts()->size(), 8u);

  static constexpr std::string_view kInputNames[7] = {"clk", "rst_n", "paddr", "pwrite", "psel", "penable", "pwdata"};
  for (std::string_view name : kInputNames) {
    const hldb::Port *const port = hldb::findByName<hldb::Port>(name, mod->getPorts());
    ASSERT_NE(port, nullptr) << "port '" << name << "'";
    EXPECT_EQ(port->getDirection(), vpiInput) << "port '" << name << "'";
    EXPECT_NE(hldb::findByName<hldb::Net>(name, mod->getNets()), nullptr) << "port '" << name << "'";
  }

  const hldb::Port *const prdata = hldb::findByName<hldb::Port>("prdata", mod->getPorts());
  ASSERT_NE(prdata, nullptr);
  EXPECT_EQ(prdata->getDirection(), vpiOutput);
  EXPECT_NE(hldb::findByName<hldb::Variable>("prdata", mod->getVariables()), nullptr)
      << "'output logic [...] prdata' has an explicit data type, so it is a Variable, not a Net";
}

TEST_F(ApbSlaveTest, MemIsA256EntryArrayWithSingleOperandSubtractRange) {
  const hldb::Module *const mod = getDutModule();
  ASSERT_NE(mod, nullptr);
  const hldb::Variable *const mem = hldb::findByName<hldb::Variable>("mem", mod->getVariables());
  ASSERT_NE(mem, nullptr);
  ASSERT_NE(mem->getTypespec(), nullptr);
  const hldb::ArrayTypespec *const arrayTs = mem->getTypespec()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(arrayTs, nullptr);
  ASSERT_NE(arrayTs->getElemTypespec(), nullptr);
  EXPECT_NE(arrayTs->getElemTypespec()->getActual<hldb::LogicTypespec>(), nullptr);

  ASSERT_NE(arrayTs->getRange(), nullptr);
  EXPECT_EQ(arrayTs->getRange()->getRightExpr(), nullptr);
  const hldb::Operation *const subtract = arrayTs->getRange()->getLeftExpr<hldb::Operation>();
  ASSERT_NE(subtract, nullptr);
  EXPECT_EQ(subtract->getOpType(), vpiSubOp);
  ASSERT_NE(subtract->getOperands(), nullptr);
  ASSERT_EQ(subtract->getOperands()->size(), 1u);
  const hldb::Constant *const size = any_cast<hldb::Constant>(subtract->getOperands()->at(0));
  ASSERT_NE(size, nullptr);
  EXPECT_EQ(size->getDecompile(), "256");
}

TEST_F(ApbSlaveTest, StateConstantsFoldToZeroOneTwo) {
  const hldb::Module *const mod = getDutModule();
  ASSERT_NE(mod, nullptr);

  static constexpr std::string_view kNames[3] = {"SETUP", "W_ENABLE", "R_ENABLE"};
  static constexpr std::string_view kValues[3] = {"0", "1", "2"};
  for (size_t i = 0; i < 3; ++i) {
    const hldb::Variable *const var = hldb::findByName<hldb::Variable>(kNames[i], mod->getVariables());
    ASSERT_NE(var, nullptr) << kNames[i];
    EXPECT_TRUE(var->getConstantVariable()) << kNames[i];
    const hldb::Constant *const value = var->getValue<hldb::Constant>();
    ASSERT_NE(value, nullptr) << kNames[i];
    EXPECT_EQ(value->getDecompile(), kValues[i]) << kNames[i];
  }
}

// --- apb_slave: always block / reset / case FSM -----------------------------

TEST_F(ApbSlaveTest, AlwaysIsSensitiveToNegedgeRstNOrPosedgeClk) {
  const hldb::Always *const always = getAlwaysProcess();
  ASSERT_NE(always, nullptr);
  const hldb::EventControl *const ec = always->getStmt<hldb::EventControl>();
  ASSERT_NE(ec, nullptr);
  const hldb::Operation *const eventOr = ec->getCondition<hldb::Operation>();
  ASSERT_NE(eventOr, nullptr);
  EXPECT_EQ(eventOr->getOpType(), vpiEventOrOp);
  ASSERT_NE(eventOr->getOperands(), nullptr);
  ASSERT_EQ(eventOr->getOperands()->size(), 2u);

  const hldb::Operation *const negedge = any_cast<hldb::Operation>(eventOr->getOperands()->at(0));
  ASSERT_NE(negedge, nullptr);
  EXPECT_EQ(negedge->getOpType(), vpiNegedgeOp);
  const hldb::Operation *const posedge = any_cast<hldb::Operation>(eventOr->getOperands()->at(1));
  ASSERT_NE(posedge, nullptr);
  EXPECT_EQ(posedge->getOpType(), vpiPosedgeOp);
}

TEST_F(ApbSlaveTest, ResetBranchClearsApbStAndPrdata) {
  const hldb::IfElse *const ifElse = getResetIfElse();
  ASSERT_NE(ifElse, nullptr);
  const hldb::Operation *const condition = ifElse->getCondition<hldb::Operation>();
  ASSERT_NE(condition, nullptr);
  EXPECT_EQ(condition->getOpType(), vpiEqOp);

  const hldb::Begin *const thenBody = any_cast<hldb::Begin>(ifElse->getStmt());
  ASSERT_NE(thenBody, nullptr);
  ASSERT_NE(thenBody->getStmts(), nullptr);
  ASSERT_EQ(thenBody->getStmts()->size(), 2u);

  static constexpr std::string_view kTargets[2] = {"apb_st", "prdata"};
  for (size_t i = 0; i < 2; ++i) {
    const hldb::Assignment *const assign = any_cast<hldb::Assignment>(thenBody->getStmts()->at(i));
    ASSERT_NE(assign, nullptr) << "reset statement index " << i;
    const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
    ASSERT_NE(lhs, nullptr) << "reset statement index " << i;
    EXPECT_EQ(lhs->getName(), kTargets[i]) << "reset statement index " << i;
    const hldb::Constant *const rhs = assign->getRhs<hldb::Constant>();
    ASSERT_NE(rhs, nullptr) << "reset statement index " << i;
    EXPECT_EQ(rhs->getDecompile(), "0") << "reset statement index " << i;
  }
}

TEST_F(ApbSlaveTest, CaseStmtOnApbStHasThreeItemsInSourceOrder) {
  const hldb::CaseStmt *const caseStmt = getApbStCaseStmt();
  ASSERT_NE(caseStmt, nullptr) << "else-branch should be a Begin containing a single CaseStmt";
  EXPECT_EQ(caseStmt->getCaseType(), vpiCaseExact);
  const hldb::RefObj *const condition = caseStmt->getCondition<hldb::RefObj>();
  ASSERT_NE(condition, nullptr);
  EXPECT_EQ(condition->getName(), "apb_st");

  ASSERT_NE(caseStmt->getCaseItems(), nullptr);
  ASSERT_EQ(caseStmt->getCaseItems()->size(), 3u);
  static constexpr std::string_view kExprNames[3] = {"SETUP", "W_ENABLE", "R_ENABLE"};
  for (size_t i = 0; i < 3; ++i) {
    const hldb::CaseItem *const item = caseStmt->getCaseItems()->at(i);
    ASSERT_NE(item, nullptr) << "case item index " << i;
    ASSERT_NE(item->getExprs(), nullptr) << "case item index " << i;
    ASSERT_EQ(item->getExprs()->size(), 1u) << "case item index " << i;
    const hldb::RefObj *const expr = any_cast<hldb::RefObj>(item->getExprs()->at(0));
    ASSERT_NE(expr, nullptr) << "case item index " << i;
    EXPECT_EQ(expr->getName(), kExprNames[i]) << "case item index " << i;
  }
}

TEST_F(ApbSlaveTest, WEnableItemWritesMemAtPaddr) {
  const hldb::CaseStmt *const caseStmt = getApbStCaseStmt();
  ASSERT_NE(caseStmt, nullptr);
  ASSERT_NE(caseStmt->getCaseItems(), nullptr);
  ASSERT_EQ(caseStmt->getCaseItems()->size(), 3u);

  const hldb::Begin *const body = any_cast<hldb::Begin>(caseStmt->getCaseItems()->at(1)->getStmt());
  ASSERT_NE(body, nullptr) << "W_ENABLE case item body should be a Begin";
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_FALSE(body->getStmts()->empty());

  const hldb::IfStmt *const guard = any_cast<hldb::IfStmt>(body->getStmts()->at(0));
  ASSERT_NE(guard, nullptr) << "'if (psel && penable && pwrite) mem[paddr] <= pwdata;' guard";
  const hldb::Begin *const guardBody = guard->getStmt<hldb::Begin>();
  ASSERT_NE(guardBody, nullptr);
  ASSERT_NE(guardBody->getStmts(), nullptr);
  ASSERT_FALSE(guardBody->getStmts()->empty());
  const hldb::Assignment *const write = any_cast<hldb::Assignment>(guardBody->getStmts()->at(0));
  ASSERT_NE(write, nullptr);

  const hldb::BitSelect *const lhs = write->getLhs<hldb::BitSelect>();
  ASSERT_NE(lhs, nullptr) << "'mem[paddr]' lhs should be a BitSelect";
  EXPECT_EQ(lhs->getName(), "mem[paddr]");
  ASSERT_NE(lhs->getPrefix(), nullptr);
  const hldb::RefObj *const prefix = any_cast<hldb::RefObj>(lhs->getPrefix());
  ASSERT_NE(prefix, nullptr);
  EXPECT_NE(prefix->getActual<hldb::Variable>(), nullptr) << "'mem' should resolve to the Variable mem";

  const hldb::RefObj *const rhs = write->getRhs<hldb::RefObj>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getName(), "pwdata");
}

TEST_F(ApbSlaveTest, REnableItemReadsMemAtPaddr) {
  const hldb::CaseStmt *const caseStmt = getApbStCaseStmt();
  ASSERT_NE(caseStmt, nullptr);
  ASSERT_NE(caseStmt->getCaseItems(), nullptr);
  ASSERT_EQ(caseStmt->getCaseItems()->size(), 3u);

  const hldb::Begin *const body = any_cast<hldb::Begin>(caseStmt->getCaseItems()->at(2)->getStmt());
  ASSERT_NE(body, nullptr) << "R_ENABLE case item body should be a Begin";
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_FALSE(body->getStmts()->empty());

  const hldb::IfStmt *const guard = any_cast<hldb::IfStmt>(body->getStmts()->at(0));
  ASSERT_NE(guard, nullptr) << "'if (psel && penable && !pwrite) prdata <= mem[paddr];' guard";
  const hldb::Begin *const guardBody = guard->getStmt<hldb::Begin>();
  ASSERT_NE(guardBody, nullptr);
  ASSERT_NE(guardBody->getStmts(), nullptr);
  ASSERT_FALSE(guardBody->getStmts()->empty());
  const hldb::Assignment *const read = any_cast<hldb::Assignment>(guardBody->getStmts()->at(0));
  ASSERT_NE(read, nullptr);

  const hldb::RefObj *const lhs = read->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "prdata");

  const hldb::BitSelect *const rhs = read->getRhs<hldb::BitSelect>();
  ASSERT_NE(rhs, nullptr) << "'mem[paddr]' rhs should be a BitSelect";
  EXPECT_EQ(rhs->getName(), "mem[paddr]");
}

// --- apb_slave_unit_test: wildcard instance / task signature ---------------

TEST_F(ApbSlaveTest, TestbenchModuleExists) { EXPECT_NE(getTestbenchModule(), nullptr); }

TEST_F(ApbSlaveTest, InstantiatesApbSlaveWithWildcardPortConnect) {
  const hldb::Module *const tb = getTestbenchModule();
  ASSERT_NE(tb, nullptr);
  ASSERT_NE(tb->getRefInstances(), nullptr);
  ASSERT_EQ(tb->getRefInstances()->size(), 1u);

  const hldb::RefInstance *const inst = tb->getRefInstances()->at(0);
  ASSERT_NE(inst, nullptr);
  EXPECT_EQ(inst->getName(), "my_apb_slave");
  ASSERT_NE(inst->getTypespec(), nullptr);
  const hldb::ModuleTypespec *const mt = inst->getTypespec()->getActual<hldb::ModuleTypespec>();
  ASSERT_NE(mt, nullptr);
  EXPECT_EQ(mt->getName(), "apb_slave");

  ASSERT_NE(inst->getPorts(), nullptr);
  ASSERT_EQ(inst->getPorts()->size(), 1u) << "'.*' elaborates as a single port entry, not per-signal connections";
  const hldb::Port *const wildcardPort = any_cast<hldb::Port>(inst->getPorts()->at(0));
  ASSERT_NE(wildcardPort, nullptr);
  const hldb::RefObj *const lowConn = wildcardPort->getLowConn<hldb::RefObj>();
  ASSERT_NE(lowConn, nullptr);
  EXPECT_EQ(lowConn->getName(), ".*");
}

TEST_F(ApbSlaveTest, WriteTaskHasFiveIODeclsWithThreeDefaults) {
  const hldb::Module *const tb = getTestbenchModule();
  ASSERT_NE(tb, nullptr);
  ASSERT_NE(tb->getTaskFuncs(), nullptr);
  const hldb::Task *const write = hldb::findByName<hldb::Task>("write", tb->getTaskFuncs());
  ASSERT_NE(write, nullptr);
  ASSERT_NE(write->getIODecls(), nullptr);
  ASSERT_EQ(write->getIODecls()->size(), 5u);

  static constexpr std::string_view kNames[5] = {"addr", "data", "back2back", "setup_psel", "setup_pwrite"};
  static constexpr std::string_view kDefaults[5] = {"", "", "0", "1", "1"};
  for (size_t i = 0; i < 5; ++i) {
    const hldb::IODecl *const decl = write->getIODecls()->at(i);
    ASSERT_NE(decl, nullptr) << "IODecl index " << i;
    EXPECT_EQ(decl->getName(), kNames[i]) << "IODecl index " << i;
    EXPECT_EQ(decl->getDirection(), vpiInput) << "IODecl index " << i;
    const hldb::Constant *const def = decl->getExpr<hldb::Constant>();
    if (kDefaults[i].empty()) {
      EXPECT_EQ(def, nullptr) << "IODecl index " << i << " should have no default expression";
    } else {
      ASSERT_NE(def, nullptr) << "IODecl index " << i << " should default to " << kDefaults[i];
      EXPECT_EQ(def->getDecompile(), kDefaults[i]) << "IODecl index " << i;
    }
  }
}

// --- compiler diagnostics ---------------------------------------------------

TEST_F(ApbSlaveTest, DesignAndTestbenchBindCleanlyDespiteThirdPartySvunitRedefinitions) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND), nullptr);
  EXPECT_NE(findError(ErrorDefinition::COMP_MULTIPLY_DEFINED_TYPEDEF), nullptr)
      << "svunit_pkg.sv/svunit_testsuite.sv/svunit_testrunner.sv redeclare 'svunit_testcase'/'svunit_testsuite' "
         "among themselves -- a pre-existing third-party issue, not caused by design.sv/testbench.sv";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
