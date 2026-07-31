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

// Tests for 11.7--signed_func.sv (tags: 11.7)
//   logic signed [7:0] a;
//
//   initial begin
//     a = $signed(4'b1000);
//   end
//
// Per IEEE 1800-2017 11.7, "$signed()" is a system function that reinterprets
// its argument's bits as signed. HLC represents the call as a SysTaskCall
// node (the same node kind used for $display etc. elsewhere in this repo),
// not as a dedicated "signed-cast" operator/expression kind.
//
// Checked:
//   - module top has exactly 1 net: "a" (LogicTypespec, vpiSigned == true,
//     vector [7:0]), not decl-assigned
//   - module-level typespecs (1): just that one signed LogicTypespec (the
//     type of "a") -- there is no second module typespec for the literal
//     argument's type (see the design-level typespec count below for where
//     that shows up instead)
//   - module has exactly 1 process: an Initial whose Begin has exactly 1
//     statement: a blocking Assignment
//   - the Assignment: lhs RefObj "a" resolving to Net "a"; rhs SysTaskCall
//     "$signed" with exactly 1 argument: Constant "4'b1000" (vpiConstType
//     == binary, vpiSize == 4, typespec LogicTypespec) -- the 4-bit literal
//     is passed through unchanged as the call's sole argument, not folded
//     or re-typed to 8 bits at this stage
//   - design-level typespecs (3): ModuleTypespec, IntTypespec (signed),
//     LogicTypespec (the argument literal's own typespec, distinct from
//     the module-level LogicTypespec that types "a")
//   - compiler emits zero errors
//
// Not checked:
//   - this file carries no $display, so there is no runtime value to check
//     even in principle (see the "-sim" sibling for that).

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/initial.h>
#include <hldb/int_typespec.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/net.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sys_task_call.h>
#include <hldb/vpi_user.h>

namespace hlc {

class SignedFuncTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "11.7--signed_func.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }
};

// --- module / nets ----------------------------------------------------------

TEST_F(SignedFuncTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(SignedFuncTest, ModuleHasOneSignedVectorNetNotDeclAssigned) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  ASSERT_EQ(top->getNets()->size(), 1u);

  const hldb::Net *const a = hldb::findByName<hldb::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(a->getValue<hldb::Constant>(), nullptr);

  const hldb::LogicTypespec *const aType = a->getTypespec<hldb::RefTypespec>()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(aType, nullptr);
  EXPECT_TRUE(aType->getSigned());
  ASSERT_NE(aType->getRanges(), nullptr);
  EXPECT_EQ(aType->getRanges()->at(0)->getLeftExpr<hldb::Constant>()->getDecompile(), "7");
  EXPECT_EQ(aType->getRanges()->at(0)->getRightExpr<hldb::Constant>()->getDecompile(), "0");
}

TEST_F(SignedFuncTest, ModuleHasOneTypespec) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTypespecs(), nullptr);
  EXPECT_EQ(top->getTypespecs()->size(), 1u);
}

// --- initial block: $signed() call -------------------------------------------

TEST_F(SignedFuncTest, InitialBlockHasOneBlockingAssignment) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getStmts(), nullptr);
  ASSERT_EQ(blk->getStmts()->size(), 1u);

  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(blk->getStmts()->at(0));
  ASSERT_NE(assign, nullptr);
  EXPECT_TRUE(assign->getBlocking());
  EXPECT_EQ(assign->getLhs<hldb::RefObj>()->getName(), "a");
}

TEST_F(SignedFuncTest, AssignmentRhsIsSignedCallOfFourBitBinaryLiteral) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(blk->getStmts()->at(0));
  ASSERT_NE(assign, nullptr);

  const hldb::SysTaskCall *const call = assign->getRhs<hldb::SysTaskCall>();
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getName(), "$signed");
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 1u);
  const hldb::Constant *const arg = any_cast<hldb::Constant>(call->getArguments()->at(0));
  ASSERT_NE(arg, nullptr);
  EXPECT_EQ(arg->getDecompile(), "4'b1000");
  EXPECT_EQ(arg->getConstType(), vpiBinaryConst);
  EXPECT_NE(arg->getTypespec<hldb::RefTypespec>()->getActual<hldb::LogicTypespec>(), nullptr);
}

// --- design-level typespecs / compiler diagnostics --------------------------

TEST_F(SignedFuncTest, DesignHasThreeTypespecs) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 3u);
}

TEST_F(SignedFuncTest, CompilerReportsZeroErrors) {
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
