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

// Tests for 11.7--signed_func-sim.sv (tags: 11.7)
//   logic signed [7:0] a;
//
//   initial begin
//     a = $signed(4'b1000);
//     $display(":assert: (-8 == %d)", a);
//   end
//
// Per IEEE 1800-2023 11.7: 4'b1000 reinterpreted as signed is -8 (the
// 4-bit two's-complement value 1000b), and assigning that into a signed
// 8-bit variable sign-extends it to -8. This is the runtime-verification
// sibling of signed_func.sv, adding the $display assertion.
//
// Per IEEE 1800-2023 20.8 ("Bit-vector system functions"), $signed is a
// system *function*: it returns a value with no side effect and (unlike
// $cast) has no task form. Used as the rhs of an assignment its return
// value is consumed, so it must be modeled as a hldb::SysFuncCall, not a
// hldb::SysTaskCall (reserved for system tasks such as $display that are
// invoked for their side effect, not their value).
//
// IEEE 1800-2023 6.7/6.8: "logic signed [7:0] a" has no net-type keyword
// (wire/tri/.../nettype), so per the standard it is a variable_declaration,
// not a net_declaration. It must be modeled as hldb::Variable, found via
// Module::getVariables(), not as hldb::Net / Module::getNets().
//
// Checked:
//   - module top has exactly 1 variable: "a" (LogicTypespec, vpiSigned ==
//     true, vector [7:0]), not decl-assigned
//   - module has exactly 1 process: an Initial whose Begin has exactly 2
//     statements: a blocking Assignment followed by a SysTaskCall
//   - the Assignment: lhs RefObj "a"; rhs SysFuncCall "$signed" with 1
//     argument: Constant "4'b1000" (vpiConstType == binary)
//   - the SysTaskCall "$display" has 2 arguments: Constant string
//     ":assert: (-8 == %d)" and RefObj "a"
//   - design-level typespecs (4): ModuleTypespec, IntTypespec (signed),
//     LogicTypespec (the argument literal's own typespec), StringTypespec
//   - compiler emits zero errors
//
// Not checked (GTEST_SKIP, with a real reason, not just "no time"):
//   - Whether "a" actually equals -8 at runtime. HLC is a compiler/
//     elaborator, not a simulator: Variable::getValue<T>() only ever
//     exposes a declaration-time initializer, and "a" is only assigned
//     inside the initial block, never at declaration -- there is no field
//     capturing the post-assignment value the ":assert:" tag is asking a
//     simulator to check.

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
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sys_func_call.h>
#include <hldb/sys_task_call.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class SignedFuncSimTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "11.7--signed_func-sim.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }
};

// --- module / variables ------------------------------------------------------

TEST_F(SignedFuncSimTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(SignedFuncSimTest, ModuleHasOneSignedVectorVariableNotDeclAssigned) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  ASSERT_EQ(top->getVariables()->size(), 1u);

  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(a->getValue<hldb::Constant>(), nullptr);

  const hldb::LogicTypespec *const aType = a->getTypespec<hldb::RefTypespec>()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(aType, nullptr);
  EXPECT_TRUE(aType->getSigned());
}

// --- initial block: $signed() call + display --------------------------------

TEST_F(SignedFuncSimTest, InitialBlockHasTwoStatements) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getStmts(), nullptr);
  ASSERT_EQ(blk->getStmts()->size(), 2u);
}

TEST_F(SignedFuncSimTest, FirstStatementAssignsSignedCallOfFourBitBinaryLiteral) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(blk->getStmts()->at(0));
  ASSERT_NE(assign, nullptr);
  EXPECT_EQ(assign->getLhs<hldb::RefObj>()->getName(), "a");

  const hldb::SysFuncCall *const call = assign->getRhs<hldb::SysFuncCall>();
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getName(), "$signed");
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 1u);
  const hldb::Constant *const arg = any_cast<hldb::Constant>(call->getArguments()->at(0));
  ASSERT_NE(arg, nullptr);
  EXPECT_EQ(arg->getDecompile(), "4'b1000");
  EXPECT_EQ(arg->getConstType(), vpiBinaryConst);
}

TEST_F(SignedFuncSimTest, SecondStatementDisplaysExpectedAValue) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(blk->getStmts()->at(1));
  ASSERT_NE(disp, nullptr);
  EXPECT_EQ(disp->getName(), "$display");
  ASSERT_NE(disp->getArguments(), nullptr);
  ASSERT_EQ(disp->getArguments()->size(), 2u);
  EXPECT_EQ(any_cast<hldb::Constant>(disp->getArguments()->at(0))->getValue(), ":assert: (-8 == %d)");
  EXPECT_EQ(any_cast<hldb::RefObj>(disp->getArguments()->at(1))->getName(), "a");
}

// --- design-level typespecs / compiler diagnostics --------------------------

TEST_F(SignedFuncSimTest, DesignHasFourTypespecs) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 4u);
}

TEST_F(SignedFuncSimTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
  EXPECT_EQ(stats.nbWarning, 0);
}

// --- the actual point of the file: runtime value ----------------------------

TEST_F(SignedFuncSimTest, AEqualsNegativeEightAtRuntime) {
  GTEST_SKIP() << "IEEE 1800-2023 11.7: $signed(4'b1000) reinterprets the "
                  "4-bit pattern 1000b as signed, giving -8, which sign-"
                  "extends into the signed 8-bit variable 'a'. HLC is a "
                  "static compiler/elaborator: Variable::getValue<T>() "
                  "only ever exposes a declaration-time initializer, and "
                  "'a' is only assigned inside the initial block, never at "
                  "declaration -- there is no field capturing its post-"
                  "assignment runtime value.";
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  const hldb::Constant *const aValue = a->getValue<hldb::Constant>();
  ASSERT_NE(aValue, nullptr) << "no field captures a's post-assignment runtime value";
  EXPECT_EQ(aValue->getDecompile(), "-8");
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
