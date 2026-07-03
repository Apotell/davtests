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
// Surelog reports EL0535 ("Illegal implicit net c") but still produces UHDM.
//
// Checked:
//   - design has module work@top
//   - module has exactly 2 explicit nets: 'a' (wire [3:0], init vpiUIntConst "8")
//     and 'b' (wire [3:0], init vpiUIntConst "5")
//   - 'c' is NOT in vpiNet — implicitly declared net has no Net node
//   - LHS RefObj "c" on the ContAssign has no vpiActual (no Net to resolve to)
//   - 1 ContAssign with RHS = vpiUnaryOrOp(vpiBitOrOp(RefObj"a", RefObj"b"))
//   - work@top has no processes
//
// Not checked:
//   - Surelog actually emitting EL0535 (can't inspect compiler messages from tests)
//   - net type of 'a' and 'b' (vpiLogic from wire [3:0] declarations)

#include <Surelog/Common/Session.h>
#include <Surelog/SourceCompile/Compiler.h>
#include <Surelog/Tests/Test.h>

#include <uhdm/Utils.h>
#include <uhdm/constant.h>
#include <uhdm/cont_assign.h>
#include <uhdm/design.h>
#include <uhdm/module.h>
#include <uhdm/net.h>
#include <uhdm/operation.h>
#include <uhdm/ref_obj.h>
#include <uhdm/vpi_user.h>

namespace SURELOG {

class ImplicitContinuousAssignment : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "6.10--implicit_continuous_assignment.hlc"});

    ASSERT_NE(m_session, nullptr) << "Session is null";
    ASSERT_NE(m_compiler, nullptr) << "Compiler is null";
    ASSERT_NE(m_design, nullptr) << "Design is null";
  }

  static void TearDownTestSuite() {
    m_design = nullptr;
    delete m_compiler;
    m_compiler = nullptr;
    delete m_session;
    m_session = nullptr;
  }
};

TEST_F(ImplicitContinuousAssignment, ModuleExists) {
  ASSERT_NE(uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules()), nullptr);
}

// ---------------------------------------------------------------------------
// Net declarations — only 'a' and 'b' are formally declared; 'c' is implicit
// ---------------------------------------------------------------------------
TEST_F(ImplicitContinuousAssignment, TwoExplicitNetsExist) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr) << "module has no nets";
  EXPECT_EQ(top->getNets()->size(), 2u)
      << "only 'a' and 'b' are formally declared; 'c' is implicit";
}

TEST_F(ImplicitContinuousAssignment, ANetExists) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  ASSERT_NE(uhdm::findByName<uhdm::Net>("a", top->getNets()), nullptr)
      << "net 'a' not found";
}

TEST_F(ImplicitContinuousAssignment, BNetExists) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  ASSERT_NE(uhdm::findByName<uhdm::Net>("b", top->getNets()), nullptr)
      << "net 'b' not found";
}

TEST_F(ImplicitContinuousAssignment, CNetNotDeclared) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(uhdm::findByName<uhdm::Net>("c", top->getNets()), nullptr)
      << "'c' should not appear in vpiNet — it was implicitly declared (EL0535)";
}

TEST_F(ImplicitContinuousAssignment, ANetInitialValueIsEight) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const a = uhdm::findByName<uhdm::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr);
  const uhdm::Constant *const init = a->getValue<uhdm::Constant>();
  ASSERT_NE(init, nullptr) << "net 'a' has no initial value";
  EXPECT_EQ(init->getDecompile(), "8");
}

TEST_F(ImplicitContinuousAssignment, BNetInitialValueIsFive) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const b = uhdm::findByName<uhdm::Net>("b", top->getNets());
  ASSERT_NE(b, nullptr);
  const uhdm::Constant *const init = b->getValue<uhdm::Constant>();
  ASSERT_NE(init, nullptr) << "net 'b' has no initial value";
  EXPECT_EQ(init->getDecompile(), "5");
}

// ---------------------------------------------------------------------------
// Continuous assignment — assign c = |(a | b)
// ---------------------------------------------------------------------------
TEST_F(ImplicitContinuousAssignment, ContAssignExists) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr) << "module has no continuous assignments";
  EXPECT_EQ(top->getContAssigns()->size(), 1u);
}

TEST_F(ImplicitContinuousAssignment, ContAssignLhsIsC) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);

  const uhdm::ContAssign *const ca = top->getContAssigns()->at(0);
  ASSERT_NE(ca, nullptr);
  const uhdm::RefObj *const lhs = ca->getLhs<uhdm::RefObj>();
  ASSERT_NE(lhs, nullptr) << "ContAssign LHS is not a RefObj";
  EXPECT_EQ(lhs->getName(), "c");
}

TEST_F(ImplicitContinuousAssignment, ContAssignLhsHasNoActual) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);

  const uhdm::RefObj *const lhs = top->getContAssigns()->at(0)->getLhs<uhdm::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getActual(), nullptr)
      << "'c' is implicit — its LHS RefObj should have no vpiActual back-pointer";
}

// ---------------------------------------------------------------------------
// RHS expression — |(a | b): unary-or wrapping a bitwise-or
// ---------------------------------------------------------------------------
TEST_F(ImplicitContinuousAssignment, ContAssignRhsIsUnaryOr) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);

  const uhdm::Operation *const rhs =
      top->getContAssigns()->at(0)->getRhs<uhdm::Operation>();
  ASSERT_NE(rhs, nullptr) << "ContAssign RHS is not an Operation";
  EXPECT_EQ(rhs->getOpType(), vpiUnaryOrOp)
      << "expected vpiUnaryOrOp (7) — reduction OR";
}

TEST_F(ImplicitContinuousAssignment, UnaryOrOperandIsBitOr) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);

  const uhdm::Operation *const outer =
      top->getContAssigns()->at(0)->getRhs<uhdm::Operation>();
  ASSERT_NE(outer, nullptr);
  ASSERT_NE(outer->getOperands(), nullptr);
  ASSERT_EQ(outer->getOperands()->size(), 1u);

  const uhdm::Operation *const inner =
      any_cast<uhdm::Operation>((*outer->getOperands())[0]);
  ASSERT_NE(inner, nullptr) << "unary-or operand is not an Operation";
  EXPECT_EQ(inner->getOpType(), vpiBitOrOp)
      << "expected vpiBitOrOp (29) — binary bitwise OR";
}

TEST_F(ImplicitContinuousAssignment, BitOrFirstOperandIsA) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);

  const uhdm::Operation *const outer =
      top->getContAssigns()->at(0)->getRhs<uhdm::Operation>();
  ASSERT_NE(outer, nullptr);
  ASSERT_NE(outer->getOperands(), nullptr);
  const uhdm::Operation *const inner =
      any_cast<uhdm::Operation>((*outer->getOperands())[0]);
  ASSERT_NE(inner, nullptr);
  ASSERT_NE(inner->getOperands(), nullptr);
  ASSERT_GE(inner->getOperands()->size(), 1u);

  const uhdm::RefObj *const a =
      any_cast<uhdm::RefObj>((*inner->getOperands())[0]);
  ASSERT_NE(a, nullptr) << "first bitwise-or operand is not a RefObj";
  EXPECT_EQ(a->getName(), "a");
}

TEST_F(ImplicitContinuousAssignment, BitOrSecondOperandIsB) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);

  const uhdm::Operation *const outer =
      top->getContAssigns()->at(0)->getRhs<uhdm::Operation>();
  ASSERT_NE(outer, nullptr);
  ASSERT_NE(outer->getOperands(), nullptr);
  const uhdm::Operation *const inner =
      any_cast<uhdm::Operation>((*outer->getOperands())[0]);
  ASSERT_NE(inner, nullptr);
  ASSERT_NE(inner->getOperands(), nullptr);
  ASSERT_GE(inner->getOperands()->size(), 2u);

  const uhdm::RefObj *const b =
      any_cast<uhdm::RefObj>((*inner->getOperands())[1]);
  ASSERT_NE(b, nullptr) << "second bitwise-or operand is not a RefObj";
  EXPECT_EQ(b->getName(), "b");
}

TEST_F(ImplicitContinuousAssignment, ANetInitialValueConstType) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const a = uhdm::findByName<uhdm::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr);
  const uhdm::Constant *const init = a->getValue<uhdm::Constant>();
  ASSERT_NE(init, nullptr);
  EXPECT_EQ(init->getConstType(), vpiUIntConst)
      << "Surelog stores unsized integer literals as vpiUIntConst (9)";
}

TEST_F(ImplicitContinuousAssignment, BNetInitialValueConstType) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const b = uhdm::findByName<uhdm::Net>("b", top->getNets());
  ASSERT_NE(b, nullptr);
  const uhdm::Constant *const init = b->getValue<uhdm::Constant>();
  ASSERT_NE(init, nullptr);
  EXPECT_EQ(init->getConstType(), vpiUIntConst)
      << "Surelog stores unsized integer literals as vpiUIntConst (9)";
}

TEST_F(ImplicitContinuousAssignment, NoProcesses) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getProcesses() == nullptr || top->getProcesses()->empty());
}

}  // namespace SURELOG

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
