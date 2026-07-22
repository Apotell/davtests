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

// Tests for 10.6.1--assign-deassign.sv (tags: 10.6.1)
//   module top(clk, q, d, clr, set);
//     input clk, d, clr, set;
//     output q;
//     logic q;
//     always @(clr or set)
//       if (clr)
//         assign q = 0;
//       else if (set)
//         assign q = 1;
//       else
//         deassign q;
//     always @(posedge clk)
//       q <= d;
//   endmodule
//
// Checked:
//   - design has module work@top with exactly 5 nets: "clk", "q", "d",
//     "clr", "set", each RefTypespec -> LogicTypespec
//   - module has exactly 5 ports: clk/d/clr/set (input), q (output)
//   - module has exactly 2 processes, both Always (vpiAlwaysType: always)
//   - Process[0]: EventControl, condition Operation (event-or) of RefObj
//     "clr"/"set"; stmt is an IfElse: condition RefObj "clr", stmt
//     AssignStmt (procedural continuous 'assign q = 0;': lhs RefObj "q",
//     rhs Constant "0"); elseStmt is a nested IfElse: condition RefObj
//     "set", stmt AssignStmt ('assign q = 1;': lhs RefObj "q", rhs Constant
//     "1"); its elseStmt is a Deassign ('deassign q;': lhs RefObj "q")
//   - Process[1]: EventControl, condition Operation (posedge) of RefObj
//     "clk"; stmt is an Assignment with getBlocking() == false (the
//     non-blocking 'q <= d;'): lhs RefObj "q", rhs RefObj "d"
//   - design-level typespecs (2): ModuleTypespec, IntTypespec (signed)
//   - compiler emits zero errors
//
// Note: this file's compiler log reports self-consistency coverage of
// 99.48% (not the 100% seen in every other file in this chapter) -- a
// small, as-yet-unexplained discrepancy worth a closer look separately;
// it is not something this test suite currently pins down to a specific
// missing field.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/always.h>
#include <hldb/assign_stmt.h>
#include <hldb/assignment.h>
#include <hldb/constant.h>
#include <hldb/deassign.h>
#include <hldb/design.h>
#include <hldb/event_control.h>
#include <hldb/if_else.h>
#include <hldb/int_typespec.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/port.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/vpi_user.h>

namespace hlc {

class AssignDeassignTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "10.6.1--assign-deassign.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("work@top", m_design->getAllModules()); }
};

// --- module / nets / ports -----------------------------------------------

TEST_F(AssignDeassignTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(AssignDeassignTest, ModuleHasFiveLogicNets) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  ASSERT_EQ(top->getNets()->size(), 5u);
  const char *const names[5] = {"clk", "q", "d", "clr", "set"};
  for (uint32_t i = 0; i < 5u; ++i) {
    const hldb::Net *const net = hldb::findByName<hldb::Net>(names[i], top->getNets());
    ASSERT_NE(net, nullptr) << "net " << names[i];
    EXPECT_NE(net->getTypespec<hldb::RefTypespec>()->getActual<hldb::LogicTypespec>(), nullptr);
  }
}

TEST_F(AssignDeassignTest, ModuleHasFivePortsWithQAsOutput) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getPorts(), nullptr);
  ASSERT_EQ(top->getPorts()->size(), 5u);
  for (const hldb::Ports *const anyPort : *top->getPorts()) {
    const hldb::Port *const port = any_cast<hldb::Port>(anyPort);
    ASSERT_NE(port, nullptr);
    if (port->getName() == "q") {
      EXPECT_EQ(port->getDirection(), vpiOutput);
    } else {
      EXPECT_EQ(port->getDirection(), vpiInput) << "port " << port->getName();
    }
  }
}

// --- process[0]: always @(clr or set) if/else-if/else assign/deassign ------

TEST_F(AssignDeassignTest, ModuleHasTwoAlwaysProcesses) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 2u);
  for (uint32_t i = 0; i < 2u; ++i) {
    const hldb::Always *const always = any_cast<hldb::Always>(top->getProcesses()->at(i));
    ASSERT_NE(always, nullptr) << "process " << i;
    EXPECT_EQ(always->getAlwaysType(), vpiAlways);
  }
}

TEST_F(AssignDeassignTest, FirstAlwaysTriggersOnClrOrSet) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Always *const always = any_cast<hldb::Always>(top->getProcesses()->at(0));
  ASSERT_NE(always, nullptr);
  const hldb::EventControl *const ec = always->getStmt<hldb::EventControl>();
  ASSERT_NE(ec, nullptr);
  const hldb::Operation *const cond = ec->getCondition<hldb::Operation>();
  ASSERT_NE(cond, nullptr);
  EXPECT_EQ(cond->getOpType(), vpiEventOrOp);
  ASSERT_NE(cond->getOperands(), nullptr);
  ASSERT_EQ(cond->getOperands()->size(), 2u);
  EXPECT_EQ(any_cast<hldb::RefObj>(cond->getOperands()->at(0))->getName(), "clr");
  EXPECT_EQ(any_cast<hldb::RefObj>(cond->getOperands()->at(1))->getName(), "set");
}

TEST_F(AssignDeassignTest, IfClrAssignsZeroToQ) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Always *const always = any_cast<hldb::Always>(top->getProcesses()->at(0));
  ASSERT_NE(always, nullptr);
  const hldb::EventControl *const ec = always->getStmt<hldb::EventControl>();
  ASSERT_NE(ec, nullptr);
  const hldb::IfElse *const outer = ec->getStmt<hldb::IfElse>();
  ASSERT_NE(outer, nullptr);
  EXPECT_EQ(outer->getCondition<hldb::RefObj>()->getName(), "clr");
  const hldb::AssignStmt *const assignZero = outer->getStmt<hldb::AssignStmt>();
  ASSERT_NE(assignZero, nullptr) << "'assign q = 0;' should elaborate as an AssignStmt";
  EXPECT_EQ(assignZero->getLhs<hldb::RefObj>()->getName(), "q");
  EXPECT_EQ(assignZero->getRhs<hldb::Constant>()->getDecompile(), "0");
}

TEST_F(AssignDeassignTest, ElseIfSetAssignsOneToQElseDeassignsQ) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Always *const always = any_cast<hldb::Always>(top->getProcesses()->at(0));
  ASSERT_NE(always, nullptr);
  const hldb::EventControl *const ec = always->getStmt<hldb::EventControl>();
  ASSERT_NE(ec, nullptr);
  const hldb::IfElse *const outer = ec->getStmt<hldb::IfElse>();
  ASSERT_NE(outer, nullptr);
  const hldb::IfElse *const inner = outer->getElseStmt<hldb::IfElse>();
  ASSERT_NE(inner, nullptr) << "'else if (set) ... else ...' should elaborate as a nested IfElse";
  EXPECT_EQ(inner->getCondition<hldb::RefObj>()->getName(), "set");
  const hldb::AssignStmt *const assignOne = inner->getStmt<hldb::AssignStmt>();
  ASSERT_NE(assignOne, nullptr) << "'assign q = 1;' should elaborate as an AssignStmt";
  EXPECT_EQ(assignOne->getLhs<hldb::RefObj>()->getName(), "q");
  EXPECT_EQ(assignOne->getRhs<hldb::Constant>()->getDecompile(), "1");
  const hldb::Deassign *const deassignQ = inner->getElseStmt<hldb::Deassign>();
  ASSERT_NE(deassignQ, nullptr) << "'deassign q;' should elaborate as a Deassign";
  EXPECT_EQ(deassignQ->getLhs<hldb::RefObj>()->getName(), "q");
}

// --- process[1]: always @(posedge clk) q <= d -------------------------------

TEST_F(AssignDeassignTest, SecondAlwaysIsNonBlockingQFromDOnPosedgeClk) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Always *const always = any_cast<hldb::Always>(top->getProcesses()->at(1));
  ASSERT_NE(always, nullptr);
  const hldb::EventControl *const ec = always->getStmt<hldb::EventControl>();
  ASSERT_NE(ec, nullptr);
  const hldb::Operation *const cond = ec->getCondition<hldb::Operation>();
  ASSERT_NE(cond, nullptr);
  EXPECT_EQ(cond->getOpType(), vpiPosedgeOp);
  ASSERT_NE(cond->getOperands(), nullptr);
  ASSERT_EQ(cond->getOperands()->size(), 1u);
  EXPECT_EQ(any_cast<hldb::RefObj>(cond->getOperands()->at(0))->getName(), "clk");

  const hldb::Assignment *const assign = ec->getStmt<hldb::Assignment>();
  ASSERT_NE(assign, nullptr);
  EXPECT_FALSE(assign->getBlocking());
  EXPECT_EQ(assign->getLhs<hldb::RefObj>()->getName(), "q");
  EXPECT_EQ(assign->getRhs<hldb::RefObj>()->getName(), "d");
}

// --- design-level typespecs / compiler diagnostics -----------------------

TEST_F(AssignDeassignTest, DesignHasTwoTypespecs) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 2u);
}

TEST_F(AssignDeassignTest, DesignHasSignedIntTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const hldb::IntTypespec *const it = any_cast<hldb::IntTypespec>(m_design->getTypespecs()->at(1));
  ASSERT_NE(it, nullptr);
  EXPECT_TRUE(it->getSigned());
}

TEST_F(AssignDeassignTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
  EXPECT_EQ(stats.nbWarning, 0);
}

// --- known gap: runtime assign/deassign override behavior requires simulation

TEST_F(AssignDeassignTest, RuntimeAssignDeassignOverrideRequiresSimulation) {
  GTEST_SKIP() << "This harness only compiles/elaborates 10.6.1--assign-deassign.sv; it does not run "
                  "a simulator, so whether the procedural continuous 'assign'/'deassign' on q actually "
                  "overrides the 'always @(posedge clk) q <= d;' drive at runtime cannot be observed "
                  "here.";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
