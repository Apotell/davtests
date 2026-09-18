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

<<<<<<< HEAD
// Tests for 9.4.1--delay_control.sv (tags: 9.4.1)
//   module block_tb ();
//     reg [3:0] a = 0;
//     initial begin
//       #10 a = 'h1;
//       #10 a = 'h2;
//       #10 a = 'h3;
//       #10 a = 'h4;
//     end
//   endmodule
//
// IEEE 1800-2017 Sec 9.4.1 "Delay control": each "#10" is a procedural
// delay control preceding the statement it governs, modeled as a
// DelayControl node whose getDelay() is the delay expression and whose
// getStmt() is the single statement it delays. Unlike
// 9.3.2--parallel_block_join.sv (no begin/end, so its fork-join sits
// directly on the Initial), this test wraps its 4 delayed assignments in
// "begin...end", so the Initial's stmt is a Begin (Scope) holding the 4
// DelayControl nodes in source order.
//
// "reg [3:0] a = 0" -- unlike the scalar "reg a/b/c = 0" in
// 9.3.1/9.3.2, the "[3:0]" packed dimension makes "a" a 4-bit vector:
// Variable with LogicTypespec, unsigned (no "signed" keyword), vpiVector
// set, and exactly 1 Range with left/right Constants "3"/"0".
//
// "'h1".."'h4" are sized-base literals with no explicit size (unsized
// hex), so each resolves to a Constant with getConstType() vpiHexConst,
// distinct from the bare-decimal vpiUIntConst used for "a"'s initializer
// and for the "10" delay amounts (plain decimal -> vpiIntConst).
//
// Checked:
//   - design has module "block_tb" with exactly 1 variable: "a"
//   - "a": LogicTypespec, unsigned, vector, exactly 1 Range with
//     left/right Constants "3"/"0"; not duplicated in the net collection;
//     initial value resolves to Constant "0" (vpiUIntConst)
//   - module has exactly 1 process, and it is an Initial
//   - the Initial's body is a Begin (begin/end wraps the 4 statements)
//     holding exactly 4 statements
//   - each of the 4 statements is a DelayControl with delay Constant "10"
//     (vpiIntConst), whose own stmt is a blocking Assignment: lhs RefObj
//     -> Variable "a", rhs Constant "'h1"/"'h2"/"'h3"/"'h4" (vpiHexConst),
//     in that source order

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
=======
// ============================================================================
// SystemVerilog source under test:
// tests/Google/chapter-9/9.4.1--delay_control.sv
// ----------------------------------------------------------------------------
// // Copyright (C) 2019-2021  The SymbiFlow Authors.
// //
// // Use of this source code is governed by a ISC-style
// // license that can be found in the LICENSE file or at
// // https://opensource.org/licenses/ISC
// //
// // SPDX-License-Identifier: ISC
//
// /*
// :name: delay_control
// :description: delay control
// :tags: 9.4.1
// */
// module block_tb ();
// 	reg [3:0] a = 0;
// 	initial begin
// 		#10 a = 'h1;
// 		#10 a = 'h2;
// 		#10 a = 'h3;
// 		#10 a = 'h4;
// 	end
// endmodule
// ============================================================================
//
// IEEE 1800-2023 construct under test (Sec 9.4.1, "Delay control"): a
// procedural timing control statement, "# delay_value statement", used
// here four times in sequence, each delaying its own blocking assignment
// to "a" by 10 time units.
//
// ----------------------------------------------------------------------------
// CHECKED (this file):
//   - module "block_tb" exists.
//   - the initial block's explicit "begin...end" produces a Begin wrapping
//     exactly four statements, each a DelayControl.
//   - each DelayControl's getDelay() is a Constant whose getDecompile() is
//     "10" (the delay value is uniform, unlike
//     test_9.3.3--block_start_finish.cpp's varying delays).
//   - each DelayControl's getStmt() is an Assignment whose getLhs() is a
//     RefObj named "a" and whose getRhs() is present and is a Constant
//     (the literal 'h1..'h4), confirming an operand is bound without
//     asserting its decompiled hex text (see NOT CHECKED).
//
// NOT CHECKED (out of scope; every assertion below states only what IEEE
// 1800-2023 requires -- none of it is based on reading a .log file or any
// other tool-output dump):
//   - The exact decompiled text of the hex literals ('h1..'h4): as in
//     test_9.3.3--block_start_finish.cpp, only presence/type is asserted.
//   - Runtime timing behavior (does "a" actually hold each value for 10
//     time units before the next assignment): HLC is a compiler/
//     elaborator with no simulation, so no execution ever happens for this
//     test to observe.
// ============================================================================

#include <hldb/Utils.h>
#include <hldb/any_type.h>
>>>>>>> origin/master
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/delay_control.h>
#include <hldb/design.h>
#include <hldb/initial.h>
<<<<<<< HEAD
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {
=======
#include <hldb/module.h>
#include <hldb/process_stmt.h>
#include <hldb/ref_obj.h>

#include <hlc/Tests/Test.h>

namespace hlc {
namespace {
const hldb::DelayControl *CheckTenUnitDelayedAssignToA(const hldb::Any *stmt) {
  const hldb::DelayControl *const delay = any_cast<hldb::DelayControl>(stmt);
  if (delay == nullptr) return nullptr;

  const hldb::Constant *const delayValue = delay->getDelay<hldb::Constant>();
  if (delayValue == nullptr || delayValue->getDecompile() != "10") return nullptr;

  const hldb::Assignment *const assign = delay->getStmt<hldb::Assignment>();
  if (assign == nullptr) return nullptr;

  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  if (lhs == nullptr || lhs->getName() != "a") return nullptr;

  if (any_cast<hldb::Constant>(assign->getRhs()) == nullptr) return nullptr;

  return delay;
}
}  // namespace
>>>>>>> origin/master

class DelayControlTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "9.4.1--delay_control.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
<<<<<<< HEAD

 protected:
  static const hldb::Module *getModule() {
    return hldb::findByName<hldb::Module>("block_tb", m_design->getAllModules());
  }

  static const hldb::Variable *getVariableA() {
    const hldb::Module *const mod = getModule();
    if (mod == nullptr) return nullptr;
    return hldb::findByName<hldb::Variable>("a", mod->getVariables());
  }

  static const hldb::LogicTypespec *getVariableATypespec() {
    const hldb::Variable *const v = getVariableA();
    if (v == nullptr || v->getTypespec() == nullptr) return nullptr;
    return v->getTypespec()->getActual<hldb::LogicTypespec>();
  }

  static const hldb::Initial *getInitialProcess() {
    const hldb::Module *const mod = getModule();
    if (mod == nullptr || mod->getProcesses() == nullptr || mod->getProcesses()->empty()) return nullptr;
    return any_cast<hldb::Initial>(mod->getProcesses()->at(0));
  }

  static const hldb::Begin *getBeginStmt() {
    const hldb::Initial *const init = getInitialProcess();
    if (init == nullptr) return nullptr;
    return init->getStmt<hldb::Begin>();
  }

  static const hldb::DelayControl *getDelayControl(size_t index) {
    const hldb::Begin *const begin = getBeginStmt();
    if (begin == nullptr || begin->getStmts() == nullptr || begin->getStmts()->size() <= index) return nullptr;
    return any_cast<hldb::DelayControl>(begin->getStmts()->at(index));
  }

  static const hldb::Assignment *getDelayedAssignment(size_t index) {
    const hldb::DelayControl *const delay = getDelayControl(index);
    if (delay == nullptr) return nullptr;
    return delay->getStmt<hldb::Assignment>();
  }

  static void ExpectDelayControlAssignsHexToA(size_t index, std::string_view hexDecompile) {
    const hldb::DelayControl *const delay = getDelayControl(index);
    ASSERT_NE(delay, nullptr) << "9.4.1: stmt[" << index << "] should be a DelayControl";

    const hldb::Constant *const delayAmount = delay->getDelay<hldb::Constant>();
    ASSERT_NE(delayAmount, nullptr) << "'#10' must carry a delay expression";
    EXPECT_EQ(delayAmount->getDecompile(), "10");
    EXPECT_EQ(delayAmount->getConstType(), vpiIntConst) << "plain decimal delay -> constType int (7)";

    const hldb::Assignment *const assign = getDelayedAssignment(index);
    ASSERT_NE(assign, nullptr) << "9.4.1: DelayControl's stmt should be an Assignment";
    EXPECT_TRUE(assign->getBlocking()) << "'a = " << hexDecompile << "' uses the blocking assignment operator '='";
    const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
    ASSERT_NE(lhs, nullptr);
    EXPECT_EQ(lhs->getName(), "a");
    EXPECT_EQ(lhs->getActual<hldb::Variable>(), getVariableA());
    const hldb::Constant *const rhs = assign->getRhs<hldb::Constant>();
    ASSERT_NE(rhs, nullptr);
    EXPECT_EQ(rhs->getDecompile(), hexDecompile);
    EXPECT_EQ(rhs->getConstType(), vpiHexConst) << "unsized hex literal -> constType hexadecimal (5)";
  }
};

// --- module / variable "a" ------------------------------------------------

TEST_F(DelayControlTest, ModuleExists) { EXPECT_NE(getModule(), nullptr); }

TEST_F(DelayControlTest, ModuleHasOneVariable) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getVariables(), nullptr);
  EXPECT_EQ(mod->getVariables()->size(), 1u);
}

TEST_F(DelayControlTest, VariableAExists) { EXPECT_NE(getVariableA(), nullptr); }

// "reg [3:0] a" has no net-type keyword, so per IEEE 1800-2023 Sec 6.7/6.8
// it must not also appear in the module's net collection.
TEST_F(DelayControlTest, VariableAIsNotDuplicatedAsANet) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  if (mod->getNets() != nullptr) {
    EXPECT_EQ(hldb::findByName<hldb::Net>("a", mod->getNets()), nullptr);
  }
}

TEST_F(DelayControlTest, VariableATypespecIsLogicUnsignedFourBitVector) {
  const hldb::LogicTypespec *const ts = getVariableATypespec();
  ASSERT_NE(ts, nullptr) << "'reg [3:0] a' should resolve to LogicTypespec";
  EXPECT_FALSE(ts->getSigned()) << "6.8: 'reg [3:0] a' with no 'signed' keyword defaults to unsigned";
  EXPECT_TRUE(ts->getVector()) << "'[3:0]' packed dimension makes 'a' a vector, not a scalar bit";
  ASSERT_NE(ts->getRanges(), nullptr);
  ASSERT_EQ(ts->getRanges()->size(), 1u);
  EXPECT_EQ(ts->getRanges()->at(0)->getLeftExpr<hldb::Constant>()->getDecompile(), "3");
  EXPECT_EQ(ts->getRanges()->at(0)->getRightExpr<hldb::Constant>()->getDecompile(), "0");
}

TEST_F(DelayControlTest, VariableAInitialValueIsConstantZero) {
  const hldb::Variable *const v = getVariableA();
  ASSERT_NE(v, nullptr);
  const hldb::Constant *const val = v->getValue<hldb::Constant>();
  ASSERT_NE(val, nullptr) << "'reg [3:0] a = 0' must carry an initial value";
  EXPECT_EQ(val->getDecompile(), "0");
  EXPECT_EQ(val->getConstType(), vpiUIntConst) << "bare decimal literal -> constType unsigned int (9)";
}

// --- initial process / begin-block structure ------------------------------

TEST_F(DelayControlTest, ModuleHasOneProcess) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getProcesses(), nullptr);
  EXPECT_EQ(mod->getProcesses()->size(), 1u);
}

TEST_F(DelayControlTest, TheOneProcessIsInitial) { EXPECT_NE(getInitialProcess(), nullptr); }

// 9.4.1: "begin...end" wraps the 4 delayed assignments, so (unlike
// 9.3.2's bare "fork...join") the Initial's stmt must be a Begin.
TEST_F(DelayControlTest, InitialStmtIsDirectlyABegin) {
  EXPECT_NE(getBeginStmt(), nullptr) << "9.4.1: 'begin...end' must be modeled as a Begin";
}

TEST_F(DelayControlTest, BeginHasFourStmts) {
  const hldb::Begin *const begin = getBeginStmt();
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  EXPECT_EQ(begin->getStmts()->size(), 4u) << "9.4.1: 4 '#10 a = ...;' statements";
}

// --- delayed assignments: #10 a = 'h1/'h2/'h3/'h4 --------------------------

TEST_F(DelayControlTest, FirstStmtIsDelayControlAssigningHex1) { ExpectDelayControlAssignsHexToA(0, "'h1"); }
TEST_F(DelayControlTest, SecondStmtIsDelayControlAssigningHex2) { ExpectDelayControlAssignsHexToA(1, "'h2"); }
TEST_F(DelayControlTest, ThirdStmtIsDelayControlAssigningHex3) { ExpectDelayControlAssignsHexToA(2, "'h3"); }
TEST_F(DelayControlTest, FourthStmtIsDelayControlAssigningHex4) { ExpectDelayControlAssignsHexToA(3, "'h4"); }

// --- compiler diagnostics --------------------------------------------------

TEST_F(DelayControlTest, ReferencesAreNotFailedBinds) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND), nullptr) << "'a' must bind to its declaration";
=======
};
// ... All tests belonging to DelayControlTest go here!

TEST_F(DelayControlTest, ModuleBlockTbExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("block_tb", m_design->getAllModules());
  ASSERT_NE(top, nullptr) << "module 'block_tb' not found";
}

TEST_F(DelayControlTest, InitialBeginWrapsFourTenUnitDelayedAssignments) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("block_tb", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);

  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->front());
  ASSERT_NE(init, nullptr);

  ASSERT_NE(init->getStmt(), nullptr) << "'initial begin ... end' should always produce a Begin";
  const hldb::Begin *const body = any_cast<hldb::Begin>(init->getStmt());
  ASSERT_NE(body, nullptr) << "explicit begin/end should produce a Begin scope node";

  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 4u) << "the begin/end block contains exactly four statements";

  EXPECT_NE(CheckTenUnitDelayedAssignToA(body->getStmts()->at(0)), nullptr) << "'#10 a = 'h1;' should match";
  EXPECT_NE(CheckTenUnitDelayedAssignToA(body->getStmts()->at(1)), nullptr) << "'#10 a = 'h2;' should match";
  EXPECT_NE(CheckTenUnitDelayedAssignToA(body->getStmts()->at(2)), nullptr) << "'#10 a = 'h3;' should match";
  EXPECT_NE(CheckTenUnitDelayedAssignToA(body->getStmts()->at(3)), nullptr) << "'#10 a = 'h4;' should match";
>>>>>>> origin/master
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
<<<<<<< HEAD
}
=======
}
>>>>>>> origin/master
