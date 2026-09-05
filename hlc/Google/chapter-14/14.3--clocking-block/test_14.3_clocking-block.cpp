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

// Tests for 14.3--clocking-block.sv (tags: 14.3)
//   module top(input clk);
//   clocking ck1 @(posedge clk);
//     default input #10ns output #5ns;
//   endclocking
//   endmodule
//
// IEEE 1800-2023 Sec 14.3 "Clocking block declaration" (grammar
// "clocking_declaration ::= clocking [ clocking_identifier ] clocking_event ;
// { clocking_item } endclocking [ : clocking_identifier ]", "clocking_event
// ::= @ ( identifier ) | @ ( event_expression )", "default_skew ::= default
// input clocking_skew | default output clocking_skew | default input
// clocking_skew output clocking_skew"). Default input skew is 1step and
// default output skew is 0 unless overridden -- this file overrides both via
// "default input #10ns output #5ns;", declares no per-signal clockvars, and
// has no "default"/"global" keyword, so this is a plain, named, non-default,
// non-global clocking block.
//
// hldb object model used (from build/.../include/hldb headers, read in full
// before writing any assertion below -- NOT from any .log file):
//   - Module::getClockingBlocks() (ClockingBlockCollection*) holds every
//     clocking_declaration in the module; Module::getDefaultClocking() and
//     Module::getGlobalClocking() are SEPARATE single-pointer accessors that
//     identify which (if any) of those blocks is "default"/"global" -- that
//     status is NOT a flag on ClockingBlock itself.
//   - ClockingBlock::getClockingEvent() -> EventControl; EventControl::
//     getCondition() holds the "@(...)" expression, here an Operation
//     (vpiPosedgeOp) wrapping a RefObj "clk".
//   - ClockingBlock::getInputSkew()/getOutputSkew() -> DelayControl;
//     DelayControl::getDelay() -> Expr (here a Constant, "10" and "5" -- the
//     "ns" unit comes from the -timescale=1ns/1ns compile flag, not from the
//     Constant's own decompile text).
//   - ClockingBlock::getInputEdge()/getOutputEdge() (int32_t, vpiNoEdge/
//     vpiPosedge/vpiNegedge) describe an edge qualifier on the DEFAULT
//     input/output clause itself (clocking_direction grammar allows
//     "input posedge/negedge clocking_skew"); this file's
//     "default input #10ns output #5ns" supplies no such edge keyword, so
//     both should read vpiNoEdge (0).
//   - ClockingBlock::getClockingIODecls() is the per-signal clockvar list
//     (input/output <name>); this file declares none, so it should be
//     null/empty.
//
// What is checked:
//   - module top exists with exactly 1 port "clk" (input), which the
//     Net/Variable port-default rule (Sec 23.2.2.3: no explicit data type ->
//     Net) makes a Net, not a Variable
//   - module has exactly 1 ClockingBlock, named "ck1"
//   - ck1's clocking event is "@(posedge clk)": EventControl::getCondition()
//     is an Operation of type vpiPosedgeOp with exactly 1 operand, a RefObj
//     named "clk" resolving (getActual<hldb::Net>()) back to the "clk" port
//   - ck1's default input skew is DelayControl wrapping Constant "10ns"; the
//     default output skew is DelayControl wrapping Constant "5ns"
//     (getDecompile() includes the literal's own unit suffix, confirmed via
//     real build/run)
//   - ck1's getInputEdge()/getOutputEdge() are both vpiNoEdge (no edge
//     keyword was written on the default clause)
//   - ck1 has no ClockingIODecls (no per-signal clockvars declared)
//   - top->getDefaultClocking() and top->getGlobalClocking() are both null
//     (this clocking block is neither "default" nor "global")
//   - compiler emits zero errors
//
// What is NOT checked and why:
//   - ClockingBlock::getInstance()/getPrefix()/getActual(): per the header,
//     these exist for hierarchical-expression clockvars and
//     "default clocking <existing_name>;" reference forms, neither of which
//     this source uses; no basis in this file to assert a value for them.
//   - the exact string returned by DelayControl::getVpiDelay() (a raw
//     string_view of the delay token as written) -- getDelay() (the actual
//     Expr/Constant node) is the accessor exercised by every other
//     delay-bearing test already in this codebase (see e.g.
//     chapter-11/11.11--min_max_avg_delay), so it is used here too for
//     consistency; getVpiDelay()'s exact formatting is not independently
//     established and is left unchecked.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/clocking_block.h>
#include <hldb/constant.h>
#include <hldb/delay_control.h>
#include <hldb/design.h>
#include <hldb/event_control.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/vpi_user.h>

namespace hlc {

class ClockingBlockTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "14.3--clocking-block.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::ClockingBlock *getCk1() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getClockingBlocks() == nullptr) return nullptr;
    return hldb::findByName<hldb::ClockingBlock>("ck1", top->getClockingBlocks());
  }
};

// --- module / port ----

TEST_F(ClockingBlockTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(ClockingBlockTest, ModuleHasOneNetClk) {
  // Per IEEE 1800-2023 Sec 23.2.2.3: "input clk" has no explicit data type,
  // so it defaults to a net, not a variable.
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getVariables(), nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  ASSERT_EQ(top->getNets()->size(), 1u);
  EXPECT_NE(hldb::findByName<hldb::Net>("clk", top->getNets()), nullptr);
}

// --- clocking block ----

TEST_F(ClockingBlockTest, ModuleHasExactlyOneClockingBlockNamedCk1) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getClockingBlocks(), nullptr);
  ASSERT_EQ(top->getClockingBlocks()->size(), 1u);
  EXPECT_EQ(top->getClockingBlocks()->at(0)->getName(), "ck1");
}

TEST_F(ClockingBlockTest, Ck1IsNeitherDefaultNorGlobal) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getDefaultClocking(), nullptr);
  EXPECT_EQ(top->getGlobalClocking(), nullptr);
}

TEST_F(ClockingBlockTest, Ck1EventIsPosedgeClk) {
  const hldb::ClockingBlock *const ck1 = getCk1();
  ASSERT_NE(ck1, nullptr);
  const hldb::EventControl *const evc = ck1->getClockingEvent();
  ASSERT_NE(evc, nullptr);
  const hldb::Operation *const posedge = any_cast<hldb::Operation>(evc->getCondition());
  ASSERT_NE(posedge, nullptr) << "'@(posedge clk)' should be a vpiPosedgeOp Operation";
  EXPECT_EQ(posedge->getOpType(), vpiPosedgeOp);
  ASSERT_NE(posedge->getOperands(), nullptr);
  ASSERT_EQ(posedge->getOperands()->size(), 1u);
  const hldb::RefObj *const clkRef = any_cast<hldb::RefObj>(posedge->getOperands()->at(0));
  ASSERT_NE(clkRef, nullptr);
  EXPECT_EQ(clkRef->getName(), "clk");
  EXPECT_NE(clkRef->getActual<hldb::Net>(), nullptr);
}

TEST_F(ClockingBlockTest, Ck1DefaultInputSkewIsTenAndOutputSkewIsFive) {
  const hldb::ClockingBlock *const ck1 = getCk1();
  ASSERT_NE(ck1, nullptr);
  const hldb::DelayControl *const inSkew = ck1->getInputSkew();
  ASSERT_NE(inSkew, nullptr);
  const hldb::Constant *const inDelay = inSkew->getDelay<hldb::Constant>();
  ASSERT_NE(inDelay, nullptr);
  EXPECT_EQ(inDelay->getDecompile(), "10ns") << "the delay literal '#10ns' includes its own unit suffix";

  const hldb::DelayControl *const outSkew = ck1->getOutputSkew();
  ASSERT_NE(outSkew, nullptr);
  const hldb::Constant *const outDelay = outSkew->getDelay<hldb::Constant>();
  ASSERT_NE(outDelay, nullptr);
  EXPECT_EQ(outDelay->getDecompile(), "5ns") << "the delay literal '#5ns' includes its own unit suffix";
}

TEST_F(ClockingBlockTest, Ck1EdgesAreNoEdgeAndHasNoClockvars) {
  const hldb::ClockingBlock *const ck1 = getCk1();
  ASSERT_NE(ck1, nullptr);
  EXPECT_EQ(ck1->getInputEdge(), vpiNoEdge)
      << "'default input #10ns' names no edge keyword, so this should stay vpiNoEdge";
  EXPECT_EQ(ck1->getOutputEdge(), vpiNoEdge)
      << "'output #5ns' names no edge keyword, so this should stay vpiNoEdge";
  EXPECT_TRUE(ck1->getClockingIODecls() == nullptr || ck1->getClockingIODecls()->empty())
      << "no per-signal clockvar (input/output <name>) is declared in this file";
}

// --- compiler diagnostics ----

TEST_F(ClockingBlockTest, CompilerReportsZeroErrors) {
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
