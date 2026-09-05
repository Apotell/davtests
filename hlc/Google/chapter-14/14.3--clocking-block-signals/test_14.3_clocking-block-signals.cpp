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

// Tests for 14.3--clocking-block-signals.sv (tags: 14.3)
//   module top(input clk, input a, output logic b, output logic c);
//   clocking ck1 @(posedge clk);
//     default input #10ns output #5ns;
//     input a;
//     output b;
//     output #3ns c;
//   endclocking
//   always_ff @(posedge clk) begin
//     b <= a;
//     c <= a;
//   end
//   endmodule
//
// IEEE 1800-2023 Sec 14.3: a clocking_item can be "clocking_direction
// list_of_clocking_decl_assign ;" (plain "input a;" / "output b;", which use
// the block's default_skew) or override the skew per signal ("output #3ns
// c;"). Sec 14.4/Table 14-1 (input/output clockvars): "input a;" makes "a"
// an input clockvar of ck1, "output b;"/"output #3ns c;" make "b"/"c" output
// clockvars. "b"/"c" are declared "output logic" at the port list, so per
// Sec 23.2.2.3 (explicit data type -> variable) they are Variables, and a
// procedural non-blocking assignment to a variable is legal (Table 10-1) --
// this file is the "good" sibling of 14.3--clocking-block-signals-error.sv,
// which uses plain "output b, output c" (no "logic") and is
// :should_fail_because: assigning to net from procedural context.
//
// hldb object model used (from headers read in full, not any .log file):
//   - ClockingBlock::getClockingIODecls() -> ClockingIODeclCollection, one
//     ClockingIODecl per "input"/"output" clocking_item line.
//   - ClockingIODecl::getDirection() (vpiInput/vpiOutput/vpiInout),
//     getInputSkew()/getOutputSkew() -> DelayControl* (populated ONLY when
//     that specific line writes its own "#<n>ns"; a plain "input a;"/
//     "output b;" with no skew token leaves these null, falling back to the
//     block's default_skew at elaboration/simulation time, not copied onto
//     the IODecl object itself), getExpr() (for hierarchical clockvar
//     overrides like "enable = top.mem1.enable" -- unused here, so null for
//     all three).
//   - hldb::Always::getAlwaysType() == vpiAlwaysFF; Always::getStmt()
//     (inherited from Process) is the EventControl for "@(posedge clk)",
//     whose EventControl::getStmt() is the Begin body.
//
// What is checked:
//   - module ports: "clk", "a" are Nets (no explicit type); "b", "c" are
//     Variables (explicit "logic")
//   - ck1 has exactly 3 ClockingIODecls, in source order: "a" (vpiInput, no
//     own skew), "b" (vpiOutput, no own skew), "c" (vpiOutput, #3ns skew)
//   - the always_ff block: getAlwaysType() == vpiAlwaysFF, body reached via
//     EventControl(@(posedge clk))->getStmt() is a Begin with 2 statements:
//     "b <= a;" (non-blocking, lhs Variable "b", rhs Net "a") and
//     "c <= a;" (non-blocking, lhs Variable "c", rhs Net "a")
//   - compiler emits zero errors (unlike the -error.sv sibling)
//
// What is NOT checked and why:
//   - ck1's own default input/output skew (#10ns/#5ns) and posedge-clk
//     event: already covered identically in 14.3--clocking-block.cpp: not
//     re-derived here to keep this file focused on the per-signal
//     clockvars and the procedural-assignment legality this file adds.
//   - ClockingIODecl::getInputEdge()/getOutputEdge() for "a"/"b"/"c": none
//     of the three lines writes an edge keyword (e.g. "input negedge a;"),
//     so there is no source text to ground an expected non-default value;
//     left unchecked rather than guessed.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/always.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/clocking_block.h>
#include <hldb/clocking_io_decl.h>
#include <hldb/constant.h>
#include <hldb/delay_control.h>
#include <hldb/design.h>
#include <hldb/event_control.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/ref_obj.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class ClockingBlockSignalsTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "14.3--clocking-block-signals.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::ClockingBlock *getCk1() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getClockingBlocks() == nullptr) return nullptr;
    return hldb::findByName<hldb::ClockingBlock>("ck1", top->getClockingBlocks());
  }

  static const hldb::Begin *getAlwaysFFBody() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getProcesses() == nullptr || top->getProcesses()->empty()) return nullptr;
    const hldb::Always *const alw = any_cast<hldb::Always>(top->getProcesses()->at(0));
    if (alw == nullptr) return nullptr;
    const hldb::EventControl *const evc = alw->getStmt<hldb::EventControl>();
    if (evc == nullptr) return nullptr;
    return evc->getStmt<hldb::Begin>();
  }
};

// --- module / ports ----

TEST_F(ClockingBlockSignalsTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(ClockingBlockSignalsTest, ClkAndAAreNetsBAndCAreVariables) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  ASSERT_EQ(top->getNets()->size(), 2u) << "'clk', 'a' have no explicit data type (Sec 23.2.2.3)";
  EXPECT_NE(hldb::findByName<hldb::Net>("clk", top->getNets()), nullptr);
  EXPECT_NE(hldb::findByName<hldb::Net>("a", top->getNets()), nullptr);

  ASSERT_NE(top->getVariables(), nullptr);
  ASSERT_EQ(top->getVariables()->size(), 2u) << "'b', 'c' are declared 'output logic' (explicit type)";
  EXPECT_NE(hldb::findByName<hldb::Variable>("b", top->getVariables()), nullptr);
  EXPECT_NE(hldb::findByName<hldb::Variable>("c", top->getVariables()), nullptr);
}

// --- clocking block clockvars ----

TEST_F(ClockingBlockSignalsTest, Ck1HasThreeClockingIODeclsInSourceOrder) {
  const hldb::ClockingBlock *const ck1 = getCk1();
  ASSERT_NE(ck1, nullptr);
  ASSERT_NE(ck1->getClockingIODecls(), nullptr);
  ASSERT_EQ(ck1->getClockingIODecls()->size(), 3u);
  const char *const expectedNames[3] = {"a", "b", "c"};
  for (uint32_t i = 0; i < 3u; ++i) {
    EXPECT_EQ(ck1->getClockingIODecls()->at(i)->getName(), expectedNames[i]) << "clockvar index " << i;
  }
}

TEST_F(ClockingBlockSignalsTest, ClockvarAIsInputWithNoOwnSkew) {
  const hldb::ClockingBlock *const ck1 = getCk1();
  ASSERT_NE(ck1, nullptr);
  ASSERT_NE(ck1->getClockingIODecls(), nullptr);
  const hldb::ClockingIODecl *const a = ck1->getClockingIODecls()->at(0);
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(a->getDirection(), vpiInput);
  EXPECT_EQ(a->getInputSkew(), nullptr) << "'input a;' writes no skew of its own -- falls back to default_skew";
  EXPECT_EQ(a->getExpr(), nullptr) << "no hierarchical clockvar override ('=') is written for 'a'";
}

TEST_F(ClockingBlockSignalsTest, ClockvarBIsOutputWithNoOwnSkew) {
  const hldb::ClockingBlock *const ck1 = getCk1();
  ASSERT_NE(ck1, nullptr);
  ASSERT_NE(ck1->getClockingIODecls(), nullptr);
  const hldb::ClockingIODecl *const b = ck1->getClockingIODecls()->at(1);
  ASSERT_NE(b, nullptr);
  EXPECT_EQ(b->getDirection(), vpiOutput);
  EXPECT_EQ(b->getOutputSkew(), nullptr) << "'output b;' writes no skew of its own -- falls back to default_skew";
}

TEST_F(ClockingBlockSignalsTest, ClockvarCIsOutputWithOwnSkewOfThree) {
  const hldb::ClockingBlock *const ck1 = getCk1();
  ASSERT_NE(ck1, nullptr);
  ASSERT_NE(ck1->getClockingIODecls(), nullptr);
  const hldb::ClockingIODecl *const c = ck1->getClockingIODecls()->at(2);
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getDirection(), vpiOutput);
  const hldb::DelayControl *const outSkew = c->getOutputSkew();
  ASSERT_NE(outSkew, nullptr) << "'output #3ns c;' overrides the default output skew";
  const hldb::Constant *const delay = outSkew->getDelay<hldb::Constant>();
  ASSERT_NE(delay, nullptr);
  EXPECT_EQ(delay->getDecompile(), "3ns") << "the delay literal '#3ns' includes its own unit suffix";
}

// --- always_ff process ----

TEST_F(ClockingBlockSignalsTest, ModuleHasExactlyOneAlwaysFFProcess) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);
  const hldb::Always *const alw = any_cast<hldb::Always>(top->getProcesses()->at(0));
  ASSERT_NE(alw, nullptr);
  EXPECT_EQ(alw->getAlwaysType(), vpiAlwaysFF);
}

TEST_F(ClockingBlockSignalsTest, AlwaysFFBodyHasTwoNonBlockingAssignments) {
  const hldb::Begin *const body = getAlwaysFFBody();
  ASSERT_NE(body, nullptr);
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 2u);

  const char *const lhsNames[2] = {"b", "c"};
  for (uint32_t i = 0; i < 2u; ++i) {
    const hldb::Assignment *const assign = any_cast<hldb::Assignment>(body->getStmts()->at(i));
    ASSERT_NE(assign, nullptr) << "stmt index " << i;
    EXPECT_FALSE(assign->getBlocking()) << "stmt index " << i;
    const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
    ASSERT_NE(lhs, nullptr) << "stmt index " << i;
    EXPECT_EQ(lhs->getName(), lhsNames[i]) << "stmt index " << i;
    EXPECT_NE(lhs->getActual<hldb::Variable>(), nullptr)
        << "'" << lhsNames[i] << "' is 'output logic' -- a Variable" << ", stmt index " << i;
    const hldb::RefObj *const rhs = assign->getRhs<hldb::RefObj>();
    ASSERT_NE(rhs, nullptr) << "stmt index " << i;
    EXPECT_EQ(rhs->getName(), "a") << "stmt index " << i;
    EXPECT_NE(rhs->getActual<hldb::Net>(), nullptr) << "'a' has no explicit type -- a Net, stmt index " << i;
  }
}

// --- compiler diagnostics ----

TEST_F(ClockingBlockSignalsTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
  EXPECT_EQ(stats.nbWarning, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
