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

// Tests for 9.2.2.1--always.sv (tags: 9.2.2.1, 9.4.1)
//   module always_tb ();
//     logic a = 0;
//     always #5 a = ~a;
//   endmodule
//
// IEEE 1800-2017 Sec 9.2.2.1 "always procedure": the general "always"
// procedure repeats its statement for the entire simulation, unlike the
// three refined forms (always_comb/9.2.2.2, always_latch/9.2.2.3,
// always_ff/9.2.2.4) that impose additional semantic checking -- this file
// uses the general, unrefined form, so its Always node's classification
// must not match any of the three refined forms' tags.
//
// Sec 9.4.1 "Delay control": "#5 a = ~a;" is a delay_control statement --
// the delay expression (5) precedes and controls the guarded statement
// (the assignment), both held directly on the DelayControl node. Confirmed
// by ctest: a delay-count literal like "#5" is classified vpiIntConst (7)
// -- the same classification this codebase already uses for a sequence's
// "##N" delay count (chapter-8/generic sequence tests) -- which is
// distinct from a plain-value literal's classification.
//
// "logic a = 0" declares a variable, never a net (same reasoning as
// 9.2.1--initial.sv): single-bit, unsigned LogicTypespec, no declared
// Ranges, in the module's variable collection only. The "0" initializer is
// a plain-value literal, not a delay/count literal, so it follows this
// codebase's established plain-value classification (vpiUIntConst), per
// chapter-5/5.7.1--integers-unsized.sv and 9.2.1--initial.sv.
//
// "~a" is IEEE 1800-2017 Sec 11.4.1's UNARY BITWISE NEGATION operator
// (flips every bit), distinct from "!" (logical negation, vpiNotOp) --
// "~" is vpiBitNegOp.
//
// Checked:
//   - design has module "always_tb" with exactly 1 variable: "a"
//   - variable "a": LogicTypespec, unsigned, no declared ranges; not
//     duplicated in the net collection; initial value resolves to
//     Constant "0" (vpiUIntConst)
//   - module has exactly 1 process, and it is an Always whose AlwaysType
//     is none of vpiAlwaysComb/vpiAlwaysFF/vpiAlwaysLatch
//   - the Always' body is directly a DelayControl (no enclosing Begin,
//     since the source has no begin/end)
//   - that DelayControl's delay resolves to Constant "5" (vpiUIntConst);
//     its wrapped stmt is the Assignment "a = ~a"
//   - the Assignment is blocking ("="); lhs is a RefObj resolving to
//     Variable "a"; rhs is an Operation(vpiBitNegOp) with 1 operand, a
//     RefObj also resolving to Variable "a"

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/always.h>
#include <hldb/assignment.h>
#include <hldb/constant.h>
#include <hldb/delay_control.h>
#include <hldb/design.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class AlwaysTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "9.2.2.1--always.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getModule() {
    return hldb::findByName<hldb::Module>("always_tb", m_design->getAllModules());
  }

  static const hldb::Variable *getVariableA() {
    const hldb::Module *const mod = getModule();
    if (mod == nullptr) return nullptr;
    return hldb::findByName<hldb::Variable>("a", mod->getVariables());
  }

  static const hldb::LogicTypespec *getVariableATypespec() {
    const hldb::Variable *const a = getVariableA();
    if (a == nullptr || a->getTypespec() == nullptr) return nullptr;
    return a->getTypespec()->getActual<hldb::LogicTypespec>();
  }

  static const hldb::Always *getAlwaysProcess() {
    const hldb::Module *const mod = getModule();
    if (mod == nullptr || mod->getProcesses() == nullptr || mod->getProcesses()->empty()) return nullptr;
    return any_cast<hldb::Always>(mod->getProcesses()->at(0));
  }

  static const hldb::DelayControl *getDelayControl() {
    const hldb::Always *const alw = getAlwaysProcess();
    if (alw == nullptr) return nullptr;
    return alw->getStmt<hldb::DelayControl>();
  }

  static const hldb::Assignment *getAssignment() {
    const hldb::DelayControl *const dc = getDelayControl();
    if (dc == nullptr) return nullptr;
    return dc->getStmt<hldb::Assignment>();
  }
};

// --- module / variable "a" ---------------------------------------------------

TEST_F(AlwaysTest, ModuleExists) { EXPECT_NE(getModule(), nullptr); }

TEST_F(AlwaysTest, ModuleHasOneVariable) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getVariables(), nullptr);
  EXPECT_EQ(mod->getVariables()->size(), 1u);
}

TEST_F(AlwaysTest, VariableAExists) { EXPECT_NE(getVariableA(), nullptr); }

// "logic a" has no net-type keyword, so per IEEE 1800-2023 Sec 6.7/6.8 it
// must not also appear in the module's net collection.
TEST_F(AlwaysTest, VariableAIsNotDuplicatedAsNet) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  if (mod->getNets() != nullptr) {
    EXPECT_EQ(hldb::findByName<hldb::Net>("a", mod->getNets()), nullptr)
        << "'logic a' has no net-type keyword and must not also appear as a Net";
  }
}

TEST_F(AlwaysTest, VariableATypespecIsLogicTypespec) { EXPECT_NE(getVariableATypespec(), nullptr); }

TEST_F(AlwaysTest, VariableATypespecIsUnsigned) {
  const hldb::LogicTypespec *const ts = getVariableATypespec();
  ASSERT_NE(ts, nullptr);
  EXPECT_FALSE(ts->getSigned()) << "6.8: 'logic' with no 'signed' keyword defaults to unsigned";
}

TEST_F(AlwaysTest, VariableATypespecHasNoDeclaredRanges) {
  const hldb::LogicTypespec *const ts = getVariableATypespec();
  ASSERT_NE(ts, nullptr);
  EXPECT_TRUE(ts->getRanges() == nullptr || ts->getRanges()->empty())
      << "'logic a' declares no '[msb:lsb]' -- it is an implicit scalar bit";
}

TEST_F(AlwaysTest, VariableAInitialValueIsConstantZero) {
  const hldb::Variable *const a = getVariableA();
  ASSERT_NE(a, nullptr);
  const hldb::Constant *const val = a->getValue<hldb::Constant>();
  ASSERT_NE(val, nullptr) << "'logic a = 0' must carry an initial value";
  EXPECT_EQ(val->getDecompile(), "0");
  EXPECT_EQ(val->getConstType(), vpiUIntConst) << "bare decimal literal -> constType unsigned int (9)";
}

// --- always process structure -------------------------------------------------

TEST_F(AlwaysTest, ModuleHasOneProcess) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getProcesses(), nullptr);
  EXPECT_EQ(mod->getProcesses()->size(), 1u);
}

TEST_F(AlwaysTest, TheOneProcessIsAlways) { EXPECT_NE(getAlwaysProcess(), nullptr); }

// 9.2.2.1: a plain "always" is the general, unrefined procedure -- its
// AlwaysType classification must not match any of the three refined forms
// defined by the later subclauses (always_comb/9.2.2.2, always_latch/
// 9.2.2.3, always_ff/9.2.2.4).
TEST_F(AlwaysTest, AlwaysTypeIsNotARefinedForm) {
  const hldb::Always *const alw = getAlwaysProcess();
  ASSERT_NE(alw, nullptr);
  EXPECT_NE(alw->getAlwaysType(), vpiAlwaysComb) << "9.2.2.1: plain 'always' is not 'always_comb'";
  EXPECT_NE(alw->getAlwaysType(), vpiAlwaysFF) << "9.2.2.1: plain 'always' is not 'always_ff'";
  EXPECT_NE(alw->getAlwaysType(), vpiAlwaysLatch) << "9.2.2.1: plain 'always' is not 'always_latch'";
}

// 9.4.1: "#5 a = ~a;" with no begin/end wraps a single statement_or_null --
// the process' stmt must be the DelayControl itself, not a Begin block.
TEST_F(AlwaysTest, AlwaysStmtIsDirectlyADelayControl) {
  EXPECT_NE(getDelayControl(), nullptr)
      << "9.4.1: a single-statement 'always' body (no begin/end) must not be wrapped in a Begin block";
}

TEST_F(AlwaysTest, DelayControlDelayIsConstantFive) {
  const hldb::DelayControl *const dc = getDelayControl();
  ASSERT_NE(dc, nullptr);
  const hldb::Constant *const delay = dc->getDelay<hldb::Constant>();
  ASSERT_NE(delay, nullptr) << "'#5' delay should resolve to a Constant";
  EXPECT_EQ(delay->getDecompile(), "5");
  EXPECT_EQ(delay->getConstType(), vpiIntConst)
      << "a delay-count literal (like a sequence '##N' delay) -> constType int (7), "
         "distinct from a plain-value literal's constType unsigned int (9)";
}

TEST_F(AlwaysTest, DelayControlWrappedStmtIsAssignment) { EXPECT_NE(getAssignment(), nullptr); }

// --- assignment "a = ~a" -----------------------------------------------------

TEST_F(AlwaysTest, AssignmentIsBlocking) {
  const hldb::Assignment *const assign = getAssignment();
  ASSERT_NE(assign, nullptr);
  EXPECT_TRUE(assign->getBlocking()) << "'a = ~a' uses the blocking assignment operator '='";
}

TEST_F(AlwaysTest, AssignmentLhsResolvesToVariableA) {
  const hldb::Assignment *const assign = getAssignment();
  ASSERT_NE(assign, nullptr);
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr) << "assignment lhs should be a RefObj";
  EXPECT_EQ(lhs->getName(), "a");
  EXPECT_EQ(lhs->getActual<hldb::Variable>(), getVariableA());
}

TEST_F(AlwaysTest, AssignmentRhsIsBitNegOperation) {
  const hldb::Assignment *const assign = getAssignment();
  ASSERT_NE(assign, nullptr);
  const hldb::Operation *const rhs = assign->getRhs<hldb::Operation>();
  ASSERT_NE(rhs, nullptr) << "'~a' should be an Operation";
  EXPECT_EQ(rhs->getOpType(), vpiBitNegOp) << "11.4.1: '~' is bitwise negation (vpiBitNegOp), not '!' (vpiNotOp)";
}

TEST_F(AlwaysTest, BitNegOperationHasOneOperand) {
  const hldb::Assignment *const assign = getAssignment();
  ASSERT_NE(assign, nullptr);
  const hldb::Operation *const rhs = assign->getRhs<hldb::Operation>();
  ASSERT_NE(rhs, nullptr);
  ASSERT_NE(rhs->getOperands(), nullptr);
  EXPECT_EQ(rhs->getOperands()->size(), 1u) << "'~a' is a unary operator with 1 operand";
}

TEST_F(AlwaysTest, BitNegOperandResolvesToVariableA) {
  const hldb::Assignment *const assign = getAssignment();
  ASSERT_NE(assign, nullptr);
  const hldb::Operation *const rhs = assign->getRhs<hldb::Operation>();
  ASSERT_NE(rhs, nullptr);
  ASSERT_NE(rhs->getOperands(), nullptr);
  ASSERT_GE(rhs->getOperands()->size(), 1u);
  const hldb::RefObj *const operand = any_cast<hldb::RefObj>((*rhs->getOperands())[0]);
  ASSERT_NE(operand, nullptr) << "operand[0] of '~a' should be a RefObj";
  EXPECT_EQ(operand->getName(), "a");
  EXPECT_EQ(operand->getActual<hldb::Variable>(), getVariableA());
}

// --- compiler diagnostics ----------------------------------------------------

// The construct with the most real risk of a binding failure is "a"
// resolving back to its declaration on either side of the assignment.
TEST_F(AlwaysTest, ReferenceToAIsNotAFailedBind) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND), nullptr)
      << "'a' in 'a = ~a' must bind to the 'logic a' declaration on both sides";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
