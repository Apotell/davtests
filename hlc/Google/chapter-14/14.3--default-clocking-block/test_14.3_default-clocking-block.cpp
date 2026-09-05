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

// Tests for 14.3--default-clocking-block.sv (tags: 14.3)
//   module top(input clk);
//   default clocking @(posedge clk);
//     default input #10ns output #5ns;
//   endclocking
//   endmodule
//
// IEEE 1800-2023 Sec 14.12 "Default clocking": "default clocking
// clocking_identifier ;" (reference form) or the inline form used here,
// "default clocking [ clocking_identifier ] clocking_event ; { clocking_item
// } endclocking" -- the clocking_identifier is OPTIONAL in the inline form,
// and this source omits it, so this clocking block is anonymous. "Only one
// default clocking can be specified in a module... Specifying more than
// once shall result in a compiler error." This file declares exactly one.
//
// hldb object model used (from headers read in full, not any .log file):
//   - default/global status is NOT a flag on ClockingBlock itself
//     (ClockingBlock::getVpiType() always returns vpiClockingBlock); instead
//     Module::getDefaultClocking() is a SEPARATE single-pointer accessor
//     that identifies which entry of Module::getClockingBlocks() (if any)
//     is the module's default. Module::getGlobalClocking() is the analogous
//     accessor for "global clocking" (Sec 14.14) and must stay null here,
//     since this file uses "default", not "global".
//   - Any::getName() on an anonymous declaration with no identifier token in
//     the source is expected to come back empty, since there is no name to
//     record.
//   - ClockingBlock::getClockingEvent()/getInputSkew()/getOutputSkew() are
//     modeled identically to the plain, named 14.3--clocking-block.sv file
//     (same "@(posedge clk)" event, same "default input #10ns output
//     #5ns;" skew line) -- re-derived independently here rather than
//     assumed, since this is a different compiled unit.
//
// What is checked:
//   - module top has exactly 1 port "clk", a Net (no explicit data type,
//     Sec 23.2.2.3)
//   - module has exactly 1 ClockingBlock, and Module::getDefaultClocking()
//     returns that same object (by pointer identity)
//   - Module::getGlobalClocking() is null (this is "default", not "global")
//   - the default clocking block's own name is empty (no identifier was
//     written after "default clocking")
//   - the default clocking block's event is "@(posedge clk)" (Operation
//     vpiPosedgeOp, 1 operand RefObj "clk" resolving to the "clk" Net)
//   - the default clocking block's default input skew is Constant "10ns",
//     default output skew is Constant "5ns" (getDecompile() includes the
//     literal's own unit suffix, confirmed via real build/run against the
//     -timescale=1ns/1ns compiled output)
//   - compiler emits zero errors (exactly one default clocking is legal)
//
// What is NOT checked and why:
//   - getInputEdge()/getOutputEdge()/getClockingIODecls(): identical
//     reasoning to 14.3--clocking-block.cpp (no edge keyword and no
//     per-signal clockvars appear in this source), covered there and not
//     re-justified per-field here beyond the direct assertions below.
//   - the "more than one default clocking is a compiler error" half of Sec
//     14.12: this source declares only one, so there is nothing in THIS
//     file to exercise that rule against; a distinct source file with two
//     "default clocking" blocks would be needed to test it, which is out of
//     scope for this .sv file.

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

class DefaultClockingBlockTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "14.3--default-clocking-block.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }
};

// --- module / port ----

TEST_F(DefaultClockingBlockTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(DefaultClockingBlockTest, ModuleHasOneNetClk) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getVariables(), nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  ASSERT_EQ(top->getNets()->size(), 1u);
  EXPECT_NE(hldb::findByName<hldb::Net>("clk", top->getNets()), nullptr);
}

// --- default clocking ----

TEST_F(DefaultClockingBlockTest, ModuleHasExactlyOneClockingBlockAndItIsTheDefault) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getClockingBlocks(), nullptr);
  ASSERT_EQ(top->getClockingBlocks()->size(), 1u);
  EXPECT_EQ(top->getDefaultClocking(), top->getClockingBlocks()->at(0));
}

TEST_F(DefaultClockingBlockTest, DefaultClockingIsNotAlsoGlobal) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getGlobalClocking(), nullptr);
}

TEST_F(DefaultClockingBlockTest, DefaultClockingBlockHasEmptyName) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getDefaultClocking(), nullptr);
  EXPECT_TRUE(top->getDefaultClocking()->getName().empty())
      << "'default clocking @(...)' with no clocking_identifier should record no name";
}

TEST_F(DefaultClockingBlockTest, DefaultClockingEventIsPosedgeClk) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::ClockingBlock *const dflt = top->getDefaultClocking();
  ASSERT_NE(dflt, nullptr);
  const hldb::EventControl *const evc = dflt->getClockingEvent();
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

TEST_F(DefaultClockingBlockTest, DefaultClockingInputSkewIsTenAndOutputSkewIsFive) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::ClockingBlock *const dflt = top->getDefaultClocking();
  ASSERT_NE(dflt, nullptr);
  const hldb::DelayControl *const inSkew = dflt->getInputSkew();
  ASSERT_NE(inSkew, nullptr);
  const hldb::Constant *const inDelay = inSkew->getDelay<hldb::Constant>();
  ASSERT_NE(inDelay, nullptr);
  EXPECT_EQ(inDelay->getDecompile(), "10ns") << "the delay literal '#10ns' includes its own unit suffix";

  const hldb::DelayControl *const outSkew = dflt->getOutputSkew();
  ASSERT_NE(outSkew, nullptr);
  const hldb::Constant *const outDelay = outSkew->getDelay<hldb::Constant>();
  ASSERT_NE(outDelay, nullptr);
  EXPECT_EQ(outDelay->getDecompile(), "5ns") << "the delay literal '#5ns' includes its own unit suffix";
}

// --- compiler diagnostics ----

TEST_F(DefaultClockingBlockTest, CompilerReportsZeroErrors) {
  // Sec 14.12: exactly one "default clocking" per module is legal.
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
