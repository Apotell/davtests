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
//   - design has module top with exactly 4 nets: "clk", "d", "clr", "set",
//     each RefTypespec -> LogicTypespec, and exactly 1 variable: "q"
//     (RefTypespec -> LogicTypespec). Per IEEE 1800-2023 Sec 6.7/6.8 (port
//     kind net-vs-variable rule): "clk"/"d"/"clr"/"set" are input ports, so
//     they always default to net regardless of data type; "q" is an
//     output port with an explicit data type ("logic q;"), so it defaults
//     to variable, not net. This is also required for the body to be
//     legal: 'assign q = 0;'/'assign q = 1;'/'deassign q;' are procedural
//     continuous assignments (Sec 10.6), and 'q <= d;' is a non-blocking
//     procedural assignment (Table 10-1) -- both assignment forms may only
//     target a variable, never a net.
//   - module has exactly 5 ports: clk/d/clr/set (input), each lowConn to
//     the matching Net; "q" (output), lowConn to the Variable "q" (not a
//     Net)
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
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class AssignDeassignTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "10.6.1--assign-deassign.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }
};

// --- module / nets / ports ----

TEST_F(AssignDeassignTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(AssignDeassignTest, ModuleHasFourLogicNetsInputsOnly) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  ASSERT_EQ(top->getNets()->size(), 4u);
  const char *const names[4] = {"clk", "d", "clr", "set"};
  for (uint32_t i = 0; i < 4u; ++i) {
    const hldb::Net *const net = hldb::findByName<hldb::Net>(names[i], top->getNets());
    ASSERT_NE(net, nullptr) << "net " << names[i];
    EXPECT_NE(net->getTypespec<hldb::RefTypespec>()->getActual<hldb::LogicTypespec>(), nullptr);
  }
  EXPECT_EQ(hldb::findByName<hldb::Net>("q", top->getNets()), nullptr)
      << "'q' is an output port with an explicit data type, so it must not appear in Nets";
}

TEST_F(AssignDeassignTest, ModuleHasOneLogicVariableQ) {
  // Per IEEE 1800-2023 Sec 6.7/6.8 (port kind net-vs-variable rule): an
  // output port with an explicit data type ("output q; logic q;") defaults
  // to variable. This is also required for legality: the procedural
  // continuous assignments ('assign'/'deassign', Sec 10.6) and the
  // non-blocking assignment ('q <= d;', Table 10-1) below may only target
  // a variable.
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  ASSERT_EQ(top->getVariables()->size(), 1u);
  const hldb::Variable *const q = hldb::findByName<hldb::Variable>("q", top->getVariables());
  ASSERT_NE(q, nullptr);
  EXPECT_NE(q->getTypespec<hldb::RefTypespec>()->getActual<hldb::LogicTypespec>(), nullptr);
}

TEST_F(AssignDeassignTest, ModuleHasFivePortsWithQAsOutput) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getPorts(), nullptr);
  ASSERT_EQ(top->getPorts()->size(), 5u);
  for (const hldb::Ports *const anyPort : *top->getPorts()) {
    const hldb::Port *const port = any_cast<hldb::Port>(anyPort);
    ASSERT_NE(port, nullptr);
    const hldb::RefObj *const lowConn = port->getLowConn<hldb::RefObj>();
    ASSERT_NE(lowConn, nullptr) << "port low_conn is a RefObj pointing at the net/variable, not the net/variable "
                                    "itself; port "
                                 << port->getName();
    EXPECT_EQ(lowConn->getName(), port->getName());
    if (port->getName() == "q") {
      EXPECT_EQ(port->getDirection(), vpiOutput);
      EXPECT_NE(lowConn->getActual<hldb::Variable>(), nullptr) << "'q' should resolve to the Variable, not a Net";
    } else {
      EXPECT_EQ(port->getDirection(), vpiInput) << "port " << port->getName();
      EXPECT_NE(lowConn->getActual<hldb::Net>(), nullptr) << "port " << port->getName();
    }
  }
}

// --- process[0]: always @(clr or set) if/else-if/else assign/deassign ----

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
  const hldb::RefObj *const lhs = assignZero->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "q");
  EXPECT_NE(lhs->getActual<hldb::Variable>(), nullptr)
      << "IEEE 1800-2023 Sec 10.6: a procedural continuous assignment ('assign') may only target a "
         "variable, never a net";
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
  const hldb::RefObj *const assignOneLhs = assignOne->getLhs<hldb::RefObj>();
  ASSERT_NE(assignOneLhs, nullptr);
  EXPECT_EQ(assignOneLhs->getName(), "q");
  EXPECT_NE(assignOneLhs->getActual<hldb::Variable>(), nullptr)
      << "IEEE 1800-2023 Sec 10.6: a procedural continuous assignment ('assign') may only target a "
         "variable, never a net";
  EXPECT_EQ(assignOne->getRhs<hldb::Constant>()->getDecompile(), "1");
  const hldb::Deassign *const deassignQ = inner->getElseStmt<hldb::Deassign>();
  ASSERT_NE(deassignQ, nullptr) << "'deassign q;' should elaborate as a Deassign";
  const hldb::RefObj *const deassignLhs = deassignQ->getLhs<hldb::RefObj>();
  ASSERT_NE(deassignLhs, nullptr);
  EXPECT_EQ(deassignLhs->getName(), "q");
  EXPECT_NE(deassignLhs->getActual<hldb::Variable>(), nullptr)
      << "IEEE 1800-2023 Sec 10.6: 'deassign' may only target a variable, never a net";
}

// --- process[1]: always @(posedge clk) q <= d ----

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
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "q");
  EXPECT_NE(lhs->getActual<hldb::Variable>(), nullptr)
      << "IEEE 1800-2023 Table 10-1: non-blocking assignment may only target a variable, never a net";
  const hldb::RefObj *const rhs = assign->getRhs<hldb::RefObj>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getName(), "d");
  EXPECT_NE(rhs->getActual<hldb::Net>(), nullptr);
}

// --- design-level typespecs / compiler diagnostics ----

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
