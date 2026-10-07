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

// Tests for 20.5--shortreal-bits-conv.sv (tags: 20.5)
//   module top();
//   	shortreal s;
//   initial begin
//   	s = $bitstoshortreal($shortrealtobits(12.45));
//   	$display(":assert: (%0d == 1)", (s > 12.449 && s < 12.451));
//   end
//   endmodule
//
// IEEE 1800-2023 Sec 20.5, "Conversion functions":
// "function [31:0] $shortrealtobits ( shortreal_val );" and
// "function shortreal $bitstoshortreal ( bit_val );" -- "$shortrealtobits
// converts values from a shortreal type to the 32-bit vector representation
// of the real number. $bitstoshortreal converts a bit pattern created by
// $shortrealtobits to a value of the shortreal type." Each takes exactly
// one argument.
//
// Checked:
//   - design has module "top" with exactly 1 variable "s" whose typespec
//     resolves to a ShortRealTypespec
//   - module has exactly 1 process, and it is an Initial
//   - the Initial's body is a Begin (from the explicit "begin ... end")
//     wrapping exactly 2 statements and no variables
//   - statement 0 is a blocking Assignment: Lhs RefObj "s" resolving to the
//     module Variable "s", Rhs SysFuncCall "$bitstoshortreal"
//   - "$bitstoshortreal" has exactly 1 argument: a nested SysFuncCall
//     "$shortrealtobits"
//   - "$shortrealtobits" has exactly 1 argument: a Constant real "12.45"
//     (size 64) whose typespec resolves to a RealTypespec
//   - statement 1 is a SysTaskCall "$display" with exactly 2 arguments: a
//     Constant string ":assert: (%0d == 1)" (size 152) and an Operation
//     vpiLogAndOp with 2 operands
//   - the "&&" operands are vpiGtOp (s > 12.449) and vpiLtOp (s < 12.451),
//     each with 2 operands: RefObj "s" resolving to the module Variable "s"
//     and a Constant real (size 64, RealTypespec)
//   - compiler reports zero errors
//
// NOT CHECKED: runtime effects (that the shortreal -> bits -> shortreal
// round trip lands within (12.449, 12.451) and that the ":assert:"
// comparison holds) cannot be observed -- HLC is a compiler/elaborator with
// no simulation capability, so no execution ever happens for this test to
// check.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/initial.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/real_typespec.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/short_real_typespec.h>
#include <hldb/string_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/sys_func_call.h>
#include <hldb/sys_task_call.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class ShortRealBitsConvFunctionTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "20.5--shortreal-bits-conv.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getModule() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::Variable *getShortRealVariable() {
    const hldb::Module *const mod = getModule();
    if (mod == nullptr || mod->getVariables() == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::Variable>("s", mod->getVariables());
  }

  static const hldb::Initial *getInitialProcess() {
    const hldb::Module *const mod = getModule();
    if (mod == nullptr || mod->getProcesses() == nullptr || mod->getProcesses()->empty()) {
      return nullptr;
    }
    return any_cast<hldb::Initial>(mod->getProcesses()->at(0));
  }

  static const hldb::Begin *getInitialBody() {
    const hldb::Initial *const init = getInitialProcess();
    if (init == nullptr) {
      return nullptr;
    }
    return init->getStmt<hldb::Begin>();
  }

  static const hldb::Assignment *getAssignment() {
    const hldb::Begin *const body = getInitialBody();
    if (body == nullptr || body->getStmts() == nullptr || body->getStmts()->empty()) {
      return nullptr;
    }
    return any_cast<hldb::Assignment>(body->getStmts()->at(0));
  }

  static const hldb::SysFuncCall *getBitsToShortRealCall() {
    const hldb::Assignment *const assign = getAssignment();
    if (assign == nullptr) {
      return nullptr;
    }
    return assign->getRhs<hldb::SysFuncCall>();
  }

  static const hldb::SysFuncCall *getShortRealToBitsCall() {
    const hldb::SysFuncCall *const outer = getBitsToShortRealCall();
    if (outer == nullptr || outer->getArguments() == nullptr || outer->getArguments()->empty()) {
      return nullptr;
    }
    return any_cast<hldb::SysFuncCall>(outer->getArguments()->at(0));
  }

  static const hldb::SysTaskCall *getDisplayCall() {
    const hldb::Begin *const body = getInitialBody();
    if (body == nullptr || body->getStmts() == nullptr || body->getStmts()->size() < 2u) {
      return nullptr;
    }
    return any_cast<hldb::SysTaskCall>(body->getStmts()->at(1));
  }

  static const hldb::Operation *getLogAndOperation() {
    const hldb::SysTaskCall *const display = getDisplayCall();
    if (display == nullptr || display->getArguments() == nullptr || display->getArguments()->size() < 2u) {
      return nullptr;
    }
    return any_cast<hldb::Operation>(display->getArguments()->at(1));
  }

  static const hldb::Operation *getComparison(size_t index) {
    const hldb::Operation *const andOp = getLogAndOperation();
    if (andOp == nullptr || andOp->getOperands() == nullptr || andOp->getOperands()->size() <= index) {
      return nullptr;
    }
    return any_cast<hldb::Operation>(andOp->getOperands()->at(index));
  }

  // Checks one "s <op> <real>" comparison: RefObj "s" bound to the module
  // Variable, and a real Constant with the given decompiled text.
  static void checkComparison(const hldb::Operation *cmp, int32_t opType, std::string_view literal) {
    ASSERT_NE(cmp, nullptr);
    EXPECT_EQ(cmp->getOpType(), opType);
    ASSERT_NE(cmp->getOperands(), nullptr);
    ASSERT_EQ(cmp->getOperands()->size(), 2u);

    const hldb::RefObj *const lhs = any_cast<hldb::RefObj>(cmp->getOperands()->at(0));
    ASSERT_NE(lhs, nullptr) << "'s' should be a RefObj";
    EXPECT_EQ(lhs->getName(), "s");
    EXPECT_EQ(lhs->getActual<hldb::Variable>(), getShortRealVariable());

    const hldb::Constant *const rhs = any_cast<hldb::Constant>(cmp->getOperands()->at(1));
    ASSERT_NE(rhs, nullptr) << "'" << literal << "' should be a Constant";
    EXPECT_EQ(rhs->getConstType(), vpiRealConst);
    EXPECT_EQ(rhs->getSize(), 64);
    EXPECT_EQ(rhs->getDecompile(), literal);
    const hldb::RefTypespec *const ref = rhs->getTypespec();
    ASSERT_NE(ref, nullptr);
    EXPECT_NE(ref->getActual<hldb::RealTypespec>(), nullptr);
  }
};

// --- module / shortreal s; ----------------------------------------------------

TEST_F(ShortRealBitsConvFunctionTest, ModuleExists) { EXPECT_NE(getModule(), nullptr); }

TEST_F(ShortRealBitsConvFunctionTest, ModuleHasOneShortRealVariable) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getVariables(), nullptr);
  EXPECT_EQ(mod->getVariables()->size(), 1u) << "'shortreal s;' is the only module variable";

  const hldb::Variable *const s = getShortRealVariable();
  ASSERT_NE(s, nullptr) << "'s' should be a Variable";
  const hldb::RefTypespec *const ref = s->getTypespec<hldb::RefTypespec>();
  ASSERT_NE(ref, nullptr);
  EXPECT_NE(ref->getActual<hldb::ShortRealTypespec>(), nullptr) << "'shortreal s' should have a ShortRealTypespec";
}

// --- initial process ------------------------------------------------------------

TEST_F(ShortRealBitsConvFunctionTest, ModuleHasOneInitialProcess) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getProcesses(), nullptr);
  EXPECT_EQ(mod->getProcesses()->size(), 1u);
  EXPECT_NE(getInitialProcess(), nullptr);
}

TEST_F(ShortRealBitsConvFunctionTest, InitialBodyIsBeginWithTwoStmts) {
  const hldb::Begin *const body = getInitialBody();
  ASSERT_NE(body, nullptr) << "'initial begin ... end' should wrap the body in a Begin";
  EXPECT_TRUE(body->getVariables() == nullptr || body->getVariables()->empty())
      << "the begin block declares no variables";
  ASSERT_NE(body->getStmts(), nullptr);
  EXPECT_EQ(body->getStmts()->size(), 2u) << "the assignment and the '$display(...);'";
}

// --- s = $bitstoshortreal($shortrealtobits(12.45)); --------------------------------

TEST_F(ShortRealBitsConvFunctionTest, Stmt0IsBlockingAssignmentToS) {
  const hldb::Assignment *const assign = getAssignment();
  ASSERT_NE(assign, nullptr) << "'s = ...;' should be an Assignment";
  EXPECT_TRUE(assign->getBlocking());

  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr) << "'s' should be a RefObj";
  EXPECT_EQ(lhs->getName(), "s");
  EXPECT_EQ(lhs->getActual<hldb::Variable>(), getShortRealVariable());

  EXPECT_NE(getBitsToShortRealCall(), nullptr) << "'$bitstoshortreal(...)' should be a SysFuncCall";
}

TEST_F(ShortRealBitsConvFunctionTest, BitsToShortRealCallHasShortRealToBitsArgument) {
  const hldb::SysFuncCall *const call = getBitsToShortRealCall();
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getName(), "$bitstoshortreal");
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 1u) << "20.5: '$bitstoshortreal' takes a single bit_val argument";
  EXPECT_NE(getShortRealToBitsCall(), nullptr) << "'$shortrealtobits(12.45)' should be a nested SysFuncCall";
}

TEST_F(ShortRealBitsConvFunctionTest, ShortRealToBitsCallHasOneRealArgument) {
  const hldb::SysFuncCall *const call = getShortRealToBitsCall();
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getName(), "$shortrealtobits");
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 1u) << "20.5: '$shortrealtobits' takes a single shortreal_val argument";

  const hldb::Constant *const arg = any_cast<hldb::Constant>(call->getArguments()->at(0));
  ASSERT_NE(arg, nullptr) << "'12.45' should be a Constant";
  EXPECT_EQ(arg->getConstType(), vpiRealConst);
  EXPECT_EQ(arg->getSize(), 64);
  EXPECT_EQ(arg->getDecompile(), "12.45");

  const hldb::RefTypespec *const ref = arg->getTypespec();
  ASSERT_NE(ref, nullptr);
  EXPECT_NE(ref->getActual<hldb::RealTypespec>(), nullptr) << "'12.45' literal should have a RealTypespec";
}

// --- $display(":assert: (%0d == 1)", (s > 12.449 && s < 12.451)); ----------------

TEST_F(ShortRealBitsConvFunctionTest, Stmt1IsDisplayWithFormatAndLogAnd) {
  const hldb::SysTaskCall *const call = getDisplayCall();
  ASSERT_NE(call, nullptr) << "'$display(...)' should be a SysTaskCall";
  EXPECT_EQ(call->getName(), "$display");
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 2u);

  const hldb::Constant *const fmt = any_cast<hldb::Constant>(call->getArguments()->at(0));
  ASSERT_NE(fmt, nullptr) << "the format string should be a Constant";
  EXPECT_EQ(fmt->getConstType(), vpiStringConst);
  EXPECT_EQ(fmt->getSize(), 152) << "19 characters * 8 bits";
  EXPECT_EQ(fmt->getValue(), ":assert: (%0d == 1)");
  const hldb::RefTypespec *const fmtRef = fmt->getTypespec();
  ASSERT_NE(fmtRef, nullptr);
  EXPECT_NE(fmtRef->getActual<hldb::StringTypespec>(), nullptr);

  const hldb::Operation *const andOp = getLogAndOperation();
  ASSERT_NE(andOp, nullptr) << "'(... && ...)' should be an Operation";
  EXPECT_EQ(andOp->getOpType(), vpiLogAndOp);
  ASSERT_NE(andOp->getOperands(), nullptr);
  EXPECT_EQ(andOp->getOperands()->size(), 2u);
}

TEST_F(ShortRealBitsConvFunctionTest, GreaterThanComparesSWith12_449) {
  checkComparison(getComparison(0), vpiGtOp, "12.449");
}

TEST_F(ShortRealBitsConvFunctionTest, LessThanComparesSWith12_451) {
  checkComparison(getComparison(1), vpiLtOp, "12.451");
}

// --- compiler diagnostics -----------------------------------------------------

TEST_F(ShortRealBitsConvFunctionTest, CompilesWithNoErrors) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
