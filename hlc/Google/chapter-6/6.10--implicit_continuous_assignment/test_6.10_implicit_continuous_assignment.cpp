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

// Validates the UHDM graph for a module that uses an implicitly-declared net
// in a continuous assignment:
//   module top();
//     wire [3:0] a = 8;  wire [3:0] b = 5;
//     assign c = |(a | b);
//   endmodule
// HLC reports EL0535 ("Illegal implicit net c") but still produces UHDM.
//
// Checked:
//   - design has module top
//   - module has exactly 2 explicit nets: 'a' (wire [3:0], init vpiUIntConst "8")
//     and 'b' (wire [3:0], init vpiUIntConst "5")
//   - 'c' is NOT in vpiNet — implicitly declared net has no Net node
//   - LHS RefObj "c" on the ContAssign has no vpiActual (no Net to resolve to)
//   - 1 ContAssign with RHS = vpiUnaryOrOp(vpiBitOrOp(RefObj"a", RefObj"b"))
//   - top has no processes
//   - net type of 'a' and 'b' is vpiWire (wire [3:0] declarations)
//   - HLC emits exactly 1 compile error (EL0535 "Illegal implicit net")

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

class ImplicitContinuousAssignment : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.10--implicit_continuous_assignment.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(ImplicitContinuousAssignment, ModuleExists) {
  ASSERT_NE(hldb::findByName<hldb::Module>("top", m_design->getAllModules()), nullptr);
}

// ---------------------------------------------------------------------------
// Net declarations — only 'a' and 'b' are formally declared; 'c' is implicit
// ---------------------------------------------------------------------------
TEST_F(ImplicitContinuousAssignment, TwoExplicitNetsExist) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr) << "module has no nets";
  EXPECT_EQ(top->getNets()->size(), 2u) << "only 'a' and 'b' are formally declared; 'c' is implicit";
}

TEST_F(ImplicitContinuousAssignment, ANetExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  ASSERT_NE(hldb::findByName<hldb::Net>("a", top->getNets()), nullptr) << "net 'a' not found";
}

TEST_F(ImplicitContinuousAssignment, BNetExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  ASSERT_NE(hldb::findByName<hldb::Net>("b", top->getNets()), nullptr) << "net 'b' not found";
}

TEST_F(ImplicitContinuousAssignment, CNetNotDeclared) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(hldb::findByName<hldb::Net>("c", top->getNets()), nullptr)
      << "'c' should not appear in vpiNet — it was implicitly declared (EL0535)";
}

TEST_F(ImplicitContinuousAssignment, ANetInitialValueIsEight) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const a = hldb::findByName<hldb::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr);
  const hldb::Constant *const init = a->getValue<hldb::Constant>();
  ASSERT_NE(init, nullptr) << "net 'a' has no initial value";
  EXPECT_EQ(init->getDecompile(), "8");
}

TEST_F(ImplicitContinuousAssignment, BNetInitialValueIsFive) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const b = hldb::findByName<hldb::Net>("b", top->getNets());
  ASSERT_NE(b, nullptr);
  const hldb::Constant *const init = b->getValue<hldb::Constant>();
  ASSERT_NE(init, nullptr) << "net 'b' has no initial value";
  EXPECT_EQ(init->getDecompile(), "5");
}

// ---------------------------------------------------------------------------
// Continuous assignment — assign c = |(a | b)
// ---------------------------------------------------------------------------
TEST_F(ImplicitContinuousAssignment, ContAssignExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr) << "module has no continuous assignments";
  EXPECT_EQ(top->getContAssigns()->size(), 1u);
}

TEST_F(ImplicitContinuousAssignment, ContAssignLhsIsC) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);

  const hldb::ContAssign *const ca = top->getContAssigns()->at(0);
  ASSERT_NE(ca, nullptr);
  const hldb::RefObj *const lhs = ca->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr) << "ContAssign LHS is not a RefObj";
  EXPECT_EQ(lhs->getName(), "c");
}

TEST_F(ImplicitContinuousAssignment, ContAssignLhsHasNoActual) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);

  const hldb::RefObj *const lhs = top->getContAssigns()->at(0)->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getActual(), nullptr) << "'c' is implicit — its LHS RefObj should have no vpiActual back-pointer";
}

// ---------------------------------------------------------------------------
// RHS expression — |(a | b): unary-or wrapping a bitwise-or
// ---------------------------------------------------------------------------
TEST_F(ImplicitContinuousAssignment, ContAssignRhsIsUnaryOr) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);

  const hldb::Operation *const rhs = top->getContAssigns()->at(0)->getRhs<hldb::Operation>();
  ASSERT_NE(rhs, nullptr) << "ContAssign RHS is not an Operation";
  EXPECT_EQ(rhs->getOpType(), vpiUnaryOrOp) << "expected vpiUnaryOrOp (7) — reduction OR";
}

TEST_F(ImplicitContinuousAssignment, UnaryOrOperandIsBitOr) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);

  const hldb::Operation *const outer = top->getContAssigns()->at(0)->getRhs<hldb::Operation>();
  ASSERT_NE(outer, nullptr);
  ASSERT_NE(outer->getOperands(), nullptr);
  ASSERT_EQ(outer->getOperands()->size(), 1u);

  const hldb::Operation *const inner = any_cast<hldb::Operation>((*outer->getOperands())[0]);
  ASSERT_NE(inner, nullptr) << "unary-or operand is not an Operation";
  EXPECT_EQ(inner->getOpType(), vpiBitOrOp) << "expected vpiBitOrOp (29) — binary bitwise OR";
}

TEST_F(ImplicitContinuousAssignment, BitOrFirstOperandIsA) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
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

TEST_F(ImplicitContinuousAssignment, BitOrSecondOperandIsB) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
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

TEST_F(ImplicitContinuousAssignment, ANetInitialValueConstType) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const a = hldb::findByName<hldb::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr);
  const hldb::Constant *const init = a->getValue<hldb::Constant>();
  ASSERT_NE(init, nullptr);
  EXPECT_EQ(init->getConstType(), vpiUIntConst) << "HLDB stores unsized integer literals as vpiUIntConst (9)";
}

TEST_F(ImplicitContinuousAssignment, BNetInitialValueConstType) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const b = hldb::findByName<hldb::Net>("b", top->getNets());
  ASSERT_NE(b, nullptr);
  const hldb::Constant *const init = b->getValue<hldb::Constant>();
  ASSERT_NE(init, nullptr);
  EXPECT_EQ(init->getConstType(), vpiUIntConst) << "HLDB stores unsized integer literals as vpiUIntConst (9)";
}

TEST_F(ImplicitContinuousAssignment, ANetTypeIsWire) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const a = hldb::findByName<hldb::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(a->getNetType(), vpiWire) << "expected vpiNetType wire (1) for 'wire [3:0] a'";
}

TEST_F(ImplicitContinuousAssignment, BNetTypeIsWire) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const b = hldb::findByName<hldb::Net>("b", top->getNets());
  ASSERT_NE(b, nullptr);
  EXPECT_EQ(b->getNetType(), vpiWire) << "expected vpiNetType wire (1) for 'wire [3:0] b'";
}

TEST_F(ImplicitContinuousAssignment, NoProcesses) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getProcesses() == nullptr || top->getProcesses()->empty());
}

// ---------------------------------------------------------------------------
// Compiler diagnostics -- HLC emits EL0535 for the implicit net 'c'
// ---------------------------------------------------------------------------
TEST_F(ImplicitContinuousAssignment, Compiler_ReportsOneError) {
  const hlc::ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbError, 1) << "expected exactly 1 EL0535 'Illegal implicit net' error";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
