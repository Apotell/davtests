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

// Tests for 14.3--global-clocking-block.sv (tags: 14.3)
//   module top(input clk);
//   global clocking ck1 @(posedge clk); endclocking
//   endmodule
//
// IEEE 1800-2023 Sec 14.14 "Global clocking": "global_clocking_declaration
// ::= global clocking [ clocking_identifier ] clocking_event ; endclocking [
// : clocking_identifier ]" -- notably, a global clocking declaration has NO
// clocking_item list in its grammar at all (unlike a plain/default clocking
// block), so "endclocking" immediately follows the ";" here; there is no
// "default input/output" line and no per-signal clockvars, by construction
// of the grammar itself, not by omission. "A given module... shall contain
// at most one global clocking declaration. Global clocking shall not be
// declared in a generate block." Purpose (per the spec): identify the
// primary clock for formal verification via $global_clock. This file
// declares exactly one, named "ck1", at module scope (not in a generate
// block).
//
// hldb object model used (from headers read in full, not any .log file):
//   Module::getGlobalClocking() is the separate accessor identifying which
//   entry of Module::getClockingBlocks() (if any) is global -- mirrors
//   Module::getDefaultClocking() used in 14.3--default-clocking-block.cpp,
//   but for the "global" keyword instead of "default". Because the global
//   clocking grammar has no clocking_item list, this ClockingBlock's
//   getClockingIODecls()/getInputSkew()/getOutputSkew() should all read
//   empty/null -- there is no "default input/output" clause and no
//   "input"/"output <name>" line anywhere in this source to populate them.
//
// What is checked:
//   - module top has exactly 1 port "clk", a Net (no explicit data type,
//     Sec 23.2.2.3)
//   - module has exactly 1 ClockingBlock, named "ck1"
//   - Module::getGlobalClocking() returns that same object (by pointer
//     identity)
//   - Module::getDefaultClocking() is null (this is "global", not
//     "default")
//   - the block's event is "@(posedge clk)" (Operation vpiPosedgeOp, 1
//     operand RefObj "clk" resolving to the "clk" Net) -- global clocking
//     still requires a clocking_event per the grammar even though it has no
//     clocking_item list
//   - the block has no default input/output skew (getInputSkew()/
//     getOutputSkew() both null) and no ClockingIODecls, and
//     getInputEdge()/getOutputEdge() both read vpiNoEdge -- all a direct
//     consequence of the global-clocking grammar having no clocking_item
//     list at all, not values this source happens to omit
//   - compiler emits zero errors (exactly one global clocking, at module
//     scope, is legal)
//
// What is NOT checked and why:
//   - the "at most one global clocking per module" and "not in a generate
//     block" rules from Sec 14.14: this source declares exactly one, at
//     plain module scope, so neither restriction is exercised by this file;
//     a second global clocking block or one nested in a generate block
//     would be needed to test those, which is out of scope for this .sv
//     file.
//   - $global_clock usage: this file never references it; Sec 14.14's
//     stated purpose for global clocking is not itself a structural fact
//     this object model can assert on without a $global_clock call site.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/clocking_block.h>
#include <hldb/design.h>
#include <hldb/event_control.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/vpi_user.h>

namespace hlc {

class GlobalClockingBlockTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "14.3--global-clocking-block.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }
};

// --- module / port ----

TEST_F(GlobalClockingBlockTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(GlobalClockingBlockTest, ModuleHasOneNetClk) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getVariables(), nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  ASSERT_EQ(top->getNets()->size(), 1u);
  EXPECT_NE(hldb::findByName<hldb::Net>("clk", top->getNets()), nullptr);
}

// --- global clocking ----

TEST_F(GlobalClockingBlockTest, ModuleHasExactlyOneClockingBlockNamedCk1AndItIsGlobal) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getClockingBlocks(), nullptr);
  ASSERT_EQ(top->getClockingBlocks()->size(), 1u);
  EXPECT_EQ(top->getClockingBlocks()->at(0)->getName(), "ck1");
  EXPECT_EQ(top->getGlobalClocking(), top->getClockingBlocks()->at(0));
}

TEST_F(GlobalClockingBlockTest, GlobalClockingIsNotAlsoDefault) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getDefaultClocking(), nullptr);
}

TEST_F(GlobalClockingBlockTest, GlobalClockingEventIsPosedgeClk) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::ClockingBlock *const glob = top->getGlobalClocking();
  ASSERT_NE(glob, nullptr);
  const hldb::EventControl *const evc = glob->getClockingEvent();
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

TEST_F(GlobalClockingBlockTest, GlobalClockingHasNoSkewNoEdgeNoClockvars) {
  // Sec 14.14's grammar has no clocking_item list for a global clocking
  // declaration at all, so none of these fields has anything to populate
  // them from.
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::ClockingBlock *const glob = top->getGlobalClocking();
  ASSERT_NE(glob, nullptr);
  EXPECT_EQ(glob->getInputSkew(), nullptr);
  EXPECT_EQ(glob->getOutputSkew(), nullptr);
  EXPECT_EQ(glob->getInputEdge(), vpiNoEdge);
  EXPECT_EQ(glob->getOutputEdge(), vpiNoEdge);
  EXPECT_TRUE(glob->getClockingIODecls() == nullptr || glob->getClockingIODecls()->empty());
}

// --- compiler diagnostics ----

TEST_F(GlobalClockingBlockTest, CompilerReportsZeroErrors) {
  // Sec 14.14: exactly one global clocking, at module scope, is legal.
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
