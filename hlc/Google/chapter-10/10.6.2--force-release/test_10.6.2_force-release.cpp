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

// Tests for 10.6.2--force-release.sv (tags: 10.6.2)
//   module top(clk, q, d, f1, f0);
//     input clk, d, f1, f0;
//     output q;
//     logic q;
//     always @(f1 or f0)
//       if (f0)
//         force q = 0;
//       else if (f1)
//         force q = 1;
//       else
//         release q;
//     always @(posedge clk)
//       q <= d;
//   endmodule
//
// Checked:
//   - design has module work@top with exactly 5 nets: "clk", "q", "d",
//     "f1", "f0", each RefTypespec -> LogicTypespec
//   - module has exactly 5 ports: clk/d/f1/f0 (input), q (output)
//   - module has exactly 2 processes, both Always (vpiAlwaysType: always)
//   - Process[0]: EventControl, condition Operation (event-or) of RefObj
//     "f1"/"f0"; stmt is an IfElse: condition RefObj "f0", stmt Force
//     ('force q = 0;': lhs RefObj "q", rhs Constant "0"); elseStmt is a
//     nested IfElse: condition RefObj "f1", stmt Force ('force q = 1;': lhs
//     RefObj "q", rhs Constant "1"); its elseStmt is a Release ('release
//     q;': lhs RefObj "q")
//   - Process[1]: EventControl, condition Operation (posedge) of RefObj
//     "clk"; stmt is an Assignment with getBlocking() == false (the
//     non-blocking 'q <= d;'): lhs RefObj "q", rhs RefObj "d"
//   - design-level typespecs (2): ModuleTypespec, IntTypespec (signed)
//   - compiler emits zero errors
//
// Note: as with 10.6.1--assign-deassign.sv, this file's compiler log
// reports self-consistency coverage of 99.44% (not the 100% seen in most
// other files in this chapter) -- the same small, unexplained discrepancy,
// not pinned down here to a specific missing field.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/always.h>
#include <hldb/assignment.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/event_control.h>
#include <hldb/force.h>
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
#include <hldb/release.h>
#include <hldb/vpi_user.h>

namespace hlc {

class ForceReleaseTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "10.6.2--force-release.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("work@top", m_design->getAllModules()); }
};

// --- module / nets / ports -----------------------------------------------

TEST_F(ForceReleaseTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(ForceReleaseTest, ModuleHasFiveLogicNets) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  ASSERT_EQ(top->getNets()->size(), 5u);
  const char *const names[5] = {"clk", "q", "d", "f1", "f0"};
  for (uint32_t i = 0; i < 5u; ++i) {
    const hldb::Net *const net = hldb::findByName<hldb::Net>(names[i], top->getNets());
    ASSERT_NE(net, nullptr) << "net " << names[i];
    EXPECT_NE(net->getTypespec<hldb::RefTypespec>()->getActual<hldb::LogicTypespec>(), nullptr);
  }
}

TEST_F(ForceReleaseTest, ModuleHasFivePortsWithQAsOutput) {
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

// --- process[0]: always @(f1 or f0) if/else-if/else force/release ----------

TEST_F(ForceReleaseTest, ModuleHasTwoAlwaysProcesses) {
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

TEST_F(ForceReleaseTest, FirstAlwaysTriggersOnF1OrF0) {
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
  EXPECT_EQ(any_cast<hldb::RefObj>(cond->getOperands()->at(0))->getName(), "f1");
  EXPECT_EQ(any_cast<hldb::RefObj>(cond->getOperands()->at(1))->getName(), "f0");
}

TEST_F(ForceReleaseTest, IfF0ForcesZeroOntoQ) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Always *const always = any_cast<hldb::Always>(top->getProcesses()->at(0));
  ASSERT_NE(always, nullptr);
  const hldb::EventControl *const ec = always->getStmt<hldb::EventControl>();
  ASSERT_NE(ec, nullptr);
  const hldb::IfElse *const outer = ec->getStmt<hldb::IfElse>();
  ASSERT_NE(outer, nullptr);
  EXPECT_EQ(outer->getCondition<hldb::RefObj>()->getName(), "f0");
  const hldb::Force *const forceZero = outer->getStmt<hldb::Force>();
  ASSERT_NE(forceZero, nullptr) << "'force q = 0;' should elaborate as a Force";
  EXPECT_EQ(forceZero->getLhs<hldb::RefObj>()->getName(), "q");
  EXPECT_EQ(forceZero->getRhs<hldb::Constant>()->getDecompile(), "0");
}

TEST_F(ForceReleaseTest, ElseIfF1ForcesOneOntoQElseReleasesQ) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Always *const always = any_cast<hldb::Always>(top->getProcesses()->at(0));
  ASSERT_NE(always, nullptr);
  const hldb::EventControl *const ec = always->getStmt<hldb::EventControl>();
  ASSERT_NE(ec, nullptr);
  const hldb::IfElse *const outer = ec->getStmt<hldb::IfElse>();
  ASSERT_NE(outer, nullptr);
  const hldb::IfElse *const inner = outer->getElseStmt<hldb::IfElse>();
  ASSERT_NE(inner, nullptr) << "'else if (f1) ... else ...' should elaborate as a nested IfElse";
  EXPECT_EQ(inner->getCondition<hldb::RefObj>()->getName(), "f1");
  const hldb::Force *const forceOne = inner->getStmt<hldb::Force>();
  ASSERT_NE(forceOne, nullptr) << "'force q = 1;' should elaborate as a Force";
  EXPECT_EQ(forceOne->getLhs<hldb::RefObj>()->getName(), "q");
  EXPECT_EQ(forceOne->getRhs<hldb::Constant>()->getDecompile(), "1");
  const hldb::Release *const releaseQ = inner->getElseStmt<hldb::Release>();
  ASSERT_NE(releaseQ, nullptr) << "'release q;' should elaborate as a Release";
  EXPECT_EQ(releaseQ->getLhs<hldb::RefObj>()->getName(), "q");
}

// --- process[1]: always @(posedge clk) q <= d -------------------------------

TEST_F(ForceReleaseTest, SecondAlwaysIsNonBlockingQFromDOnPosedgeClk) {
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

TEST_F(ForceReleaseTest, DesignHasTwoTypespecs) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 2u);
}

TEST_F(ForceReleaseTest, DesignHasSignedIntTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const hldb::IntTypespec *const it = any_cast<hldb::IntTypespec>(m_design->getTypespecs()->at(1));
  ASSERT_NE(it, nullptr);
  EXPECT_TRUE(it->getSigned());
}

TEST_F(ForceReleaseTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
  EXPECT_EQ(stats.nbWarning, 0);
}

// --- known gap: runtime force/release override behavior requires simulation

TEST_F(ForceReleaseTest, RuntimeForceReleaseOverrideRequiresSimulation) {
  GTEST_SKIP() << "This harness only compiles/elaborates 10.6.2--force-release.sv; it does not run a "
                  "simulator, so whether 'force'/'release' on q actually overrides the "
                  "'always @(posedge clk) q <= d;' drive at runtime cannot be observed here.";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
