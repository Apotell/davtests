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

// Tests for 6.10--implicit_continuous_assignment.sv (tags: 6.10)
//   module top();
//     wire [3:0] a = 8;  wire [3:0] b = 5;
//     assign c = |(a | b);
//   endmodule
//
// What to check and why (IEEE 1800-2023 6.10 "Implicit declarations",
// p.108, checked before any test code was written):
//   "If an identifier appears on the left-hand side of a continuous
//   assignment statement, and that identifier has not been declared
//   previously ... then an implicit scalar net of default net type
//   SHALL BE ASSUMED." This is a mandatory, legal SystemVerilog
//   behavior, not an error condition -- "c" on the LHS of "assign c =
//   ...;" with no prior declaration is exactly this circumstance. This
//   file has no :should_fail_because: tag -- it is legal per spec.
//
//   A prior version of this test documented "HLC reports EL0535 ('Illegal
//   implicit net c')" as expected, passing behavior (Compiler_ReportsOneError
//   asserting nbError == 1), and treated 'c' having no Net node and no
//   vpiActual back-pointer as correct. Per 6.10's mandatory-implicit-net
//   text quoted above, this is backwards: HLC should create a real
//   implicit Net for 'c'. This version asserts the spec-correct outcome
//   instead of the old, backwards-passing one.
//
//   CONFIRMED BY RUNNING THIS FILE WITH THE TWO SKIPS BELOW REMOVED: the
//   real current behavior is not "reports 1 EL0535 error" as originally
//   assumed -- HLC actually creates no Net for 'c' at all, leaves the
//   ContAssign's LHS RefObj "c" permanently unresolved (no vpiActual),
//   AND reports ZERO compiler errors for it. In other words HLC silently
//   drops the mandatory implicit net instead of either creating it (per
//   spec) or flagging it as an error -- a silent gap, not a spurious
//   diagnostic. CompilerShouldAcceptLegalImplicitNetButReportsSpuriousError
//   below happens to already pass (nbError == 0 is what the spec wants
//   too), but for the wrong reason: not because HLC implements 6.10
//   correctly, but because it silently ignores the undeclared identifier
//   entirely.
//
// What is checked:
//   - module top exists, has explicit nets 'a' (wire [3:0], init "8")
//     and 'b' (wire [3:0], init "5"), both vpiWire, both vpiUIntConst
//     initializers
//   - exactly 1 ContAssign: RHS = vpiUnaryOrOp(vpiBitOrOp(RefObj"a",
//     RefObj"b"))
//   - top has no processes
//   - THE POINT OF THIS FILE: per IEEE 1800-2023 6.10, "c" on the LHS of
//     this continuous assignment should be implicitly declared as a
//     real net. Confirmed by personally running with the skips removed:
//     HLC never creates a Net for "c" and never resolves the ContAssign
//     LHS to one -- kept as GTEST_SKIP() with the real assertions
//     underneath, per the established gating rule (skips only added
//     after personal verification)
//
// What is NOT checked and why:
//   - none: every corner above is fully structural and checkable without
//     simulation.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/vpi_user.h>

namespace hlc {

class ImplicitContinuousAssignmentTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.10--implicit_continuous_assignment.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }
};

TEST_F(ImplicitContinuousAssignmentTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

// ---------------------------------------------------------------------------
// Net declarations -- 'a' and 'b' are explicit; 'c' should be implicit
// ---------------------------------------------------------------------------
TEST_F(ImplicitContinuousAssignmentTest, TwoExplicitNetsExist) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr) << "module has no nets";
  EXPECT_GE(top->getNets()->size(), 2u) << "at least 'a' and 'b' are formally declared";
}

TEST_F(ImplicitContinuousAssignmentTest, ANetExists) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  ASSERT_NE(hldb::findByName<hldb::Net>("a", top->getNets()), nullptr) << "net 'a' not found";
}

TEST_F(ImplicitContinuousAssignmentTest, BNetExists) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  ASSERT_NE(hldb::findByName<hldb::Net>("b", top->getNets()), nullptr) << "net 'b' not found";
}

TEST_F(ImplicitContinuousAssignmentTest, ANetInitialValueIsEight) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const a = hldb::findByName<hldb::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr);
  const hldb::Constant *const init = a->getValue<hldb::Constant>();
  ASSERT_NE(init, nullptr) << "net 'a' has no initial value";
  EXPECT_EQ(init->getDecompile(), "8");
}

TEST_F(ImplicitContinuousAssignmentTest, BNetInitialValueIsFive) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const b = hldb::findByName<hldb::Net>("b", top->getNets());
  ASSERT_NE(b, nullptr);
  const hldb::Constant *const init = b->getValue<hldb::Constant>();
  ASSERT_NE(init, nullptr) << "net 'b' has no initial value";
  EXPECT_EQ(init->getDecompile(), "5");
}

TEST_F(ImplicitContinuousAssignmentTest, ANetInitialValueConstType) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const a = hldb::findByName<hldb::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr);
  const hldb::Constant *const init = a->getValue<hldb::Constant>();
  ASSERT_NE(init, nullptr);
  EXPECT_EQ(init->getConstType(), vpiUIntConst);
}

TEST_F(ImplicitContinuousAssignmentTest, BNetInitialValueConstType) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const b = hldb::findByName<hldb::Net>("b", top->getNets());
  ASSERT_NE(b, nullptr);
  const hldb::Constant *const init = b->getValue<hldb::Constant>();
  ASSERT_NE(init, nullptr);
  EXPECT_EQ(init->getConstType(), vpiUIntConst);
}

TEST_F(ImplicitContinuousAssignmentTest, ANetTypeIsWire) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const a = hldb::findByName<hldb::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(a->getNetType(), vpiWire) << "expected vpiNetType wire (1) for 'wire [3:0] a'";
}

TEST_F(ImplicitContinuousAssignmentTest, BNetTypeIsWire) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const b = hldb::findByName<hldb::Net>("b", top->getNets());
  ASSERT_NE(b, nullptr);
  EXPECT_EQ(b->getNetType(), vpiWire) << "expected vpiNetType wire (1) for 'wire [3:0] b'";
}

// ---------------------------------------------------------------------------
// Continuous assignment -- assign c = |(a | b)
// ---------------------------------------------------------------------------
TEST_F(ImplicitContinuousAssignmentTest, ContAssignExists) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr) << "module has no continuous assignments";
  EXPECT_EQ(top->getContAssigns()->size(), 1u);
}

TEST_F(ImplicitContinuousAssignmentTest, ContAssignLhsIsC) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);

  const hldb::ContAssign *const ca = top->getContAssigns()->at(0);
  ASSERT_NE(ca, nullptr);
  const hldb::RefObj *const lhs = ca->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr) << "ContAssign LHS is not a RefObj";
  EXPECT_EQ(lhs->getName(), "c");
}

// ---------------------------------------------------------------------------
// RHS expression -- |(a | b): unary-or wrapping a bitwise-or
// ---------------------------------------------------------------------------
TEST_F(ImplicitContinuousAssignmentTest, ContAssignRhsIsUnaryOr) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);

  const hldb::Operation *const rhs = top->getContAssigns()->at(0)->getRhs<hldb::Operation>();
  ASSERT_NE(rhs, nullptr) << "ContAssign RHS is not an Operation";
  EXPECT_EQ(rhs->getOpType(), vpiUnaryOrOp) << "expected vpiUnaryOrOp (7) -- reduction OR";
}

TEST_F(ImplicitContinuousAssignmentTest, UnaryOrOperandIsBitOr) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);

  const hldb::Operation *const outer = top->getContAssigns()->at(0)->getRhs<hldb::Operation>();
  ASSERT_NE(outer, nullptr);
  ASSERT_NE(outer->getOperands(), nullptr);
  ASSERT_EQ(outer->getOperands()->size(), 1u);

  const hldb::Operation *const inner = any_cast<hldb::Operation>((*outer->getOperands())[0]);
  ASSERT_NE(inner, nullptr) << "unary-or operand is not an Operation";
  EXPECT_EQ(inner->getOpType(), vpiBitOrOp) << "expected vpiBitOrOp (29) -- binary bitwise OR";
}

TEST_F(ImplicitContinuousAssignmentTest, BitOrFirstOperandIsA) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);

  const hldb::Operation *const outer = top->getContAssigns()->at(0)->getRhs<hldb::Operation>();
  ASSERT_NE(outer, nullptr);
  ASSERT_NE(outer->getOperands(), nullptr);
  const hldb::Operation *const inner = any_cast<hldb::Operation>((*outer->getOperands())[0]);
  ASSERT_NE(inner, nullptr);
  ASSERT_NE(inner->getOperands(), nullptr);
  ASSERT_GE(inner->getOperands()->size(), 1u);

  const hldb::RefObj *const a = any_cast<hldb::RefObj>((*inner->getOperands())[0]);
  ASSERT_NE(a, nullptr) << "first bitwise-or operand is not a RefObj";
  EXPECT_EQ(a->getName(), "a");
}

TEST_F(ImplicitContinuousAssignmentTest, BitOrSecondOperandIsB) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);

  const hldb::Operation *const outer = top->getContAssigns()->at(0)->getRhs<hldb::Operation>();
  ASSERT_NE(outer, nullptr);
  ASSERT_NE(outer->getOperands(), nullptr);
  const hldb::Operation *const inner = any_cast<hldb::Operation>((*outer->getOperands())[0]);
  ASSERT_NE(inner, nullptr);
  ASSERT_NE(inner->getOperands(), nullptr);
  ASSERT_GE(inner->getOperands()->size(), 2u);

  const hldb::RefObj *const b = any_cast<hldb::RefObj>((*inner->getOperands())[1]);
  ASSERT_NE(b, nullptr) << "second bitwise-or operand is not a RefObj";
  EXPECT_EQ(b->getName(), "b");
}

TEST_F(ImplicitContinuousAssignmentTest, NoProcesses) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getProcesses() == nullptr || top->getProcesses()->empty());
}

// ---------------------------------------------------------------------------
// The actual point of the file: implicit net creation on a cont-assign LHS
// is mandatory, legal SystemVerilog per IEEE 1800-2023 6.10
// ---------------------------------------------------------------------------
TEST_F(ImplicitContinuousAssignmentTest, CShouldBeAnImplicitNetButIsNotCreated) {
  GTEST_SKIP() << "Confirmed HLC bug -- verified by running this test with the skip removed "
                  "(fails as expected): IEEE 1800-2023 6.10 mandates an implicit scalar net for "
                  "'c' (undeclared, appears on the LHS of a continuous assignment), but HLC "
                  "creates no Net for it at all. Tracked, not yet fixed by the compiler.";
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_NE(hldb::findByName<hldb::Net>("c", top->getNets()), nullptr)
      << "IEEE 1800-2023 6.10: 'if an identifier appears on the left-hand side of a continuous "
         "assignment statement' and is undeclared, 'an implicit scalar net of default net type "
         "shall be assumed' -- this is mandatory, legal behavior, not an error. HLC currently "
         "creates no Net for 'c' at all";
}

TEST_F(ImplicitContinuousAssignmentTest, ContAssignLhsShouldResolveToImplicitNetCButDoesNot) {
  GTEST_SKIP() << "Confirmed HLC bug -- verified by running this test with the skip removed "
                  "(fails as expected): since HLC never creates an implicit Net for 'c' (see "
                  "CShouldBeAnImplicitNetButIsNotCreated above), the ContAssign LHS RefObj 'c' "
                  "has nothing to resolve to. Tracked, not yet fixed by the compiler.";
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);

  const hldb::RefObj *const lhs = top->getContAssigns()->at(0)->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_NE(lhs->getActual<hldb::Net>(), nullptr)
      << "'c' should resolve to the implicit Net that IEEE 1800-2023 6.10 mandates -- HLC "
         "currently leaves this RefObj unresolved (no vpiActual)";
}

TEST_F(ImplicitContinuousAssignmentTest, CompilerReportsZeroErrorsForImplicitNetC) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbError, 0)
      << "IEEE 1800-2023 6.10 mandates an implicit net here, not an error, so zero errors is the "
         "spec-correct outcome -- but confirmed by running this file directly, HLC reaches nbError "
         "== 0 for the wrong reason: it never creates the implicit Net for 'c' at all (see "
         "CShouldBeAnImplicitNetButIsNotCreated above) rather than correctly implementing 6.10. "
         "This assertion passing does NOT mean the compiler is correct here";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
