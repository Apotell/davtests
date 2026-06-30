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

// Validates the UHDM graph for a module using the cast operator:
//   module top();
//     int a = int'(2.1 * 3.7);
//   endmodule
//
// Checked:
//   - design has module work@top
//   - module has exactly 1 net: 'a' (IntTypespec)
//   - 'a' vpiValue = vpiCastOp Operation; cast typespec → IntTypespec
//   - cast has 1 operand: vpiMultOp(vpiRealConst "2.1", vpiRealConst "3.7")
//   - work@top has no continuous assignments (inline init stored as vpiValue, not ContAssign)
//   - work@top has no processes
//
// Not checked:
//   - actual evaluated result (int'(2.1 * 3.7) = 7, truncation; runtime-only)

#include <Surelog/Common/Session.h>
#include <Surelog/SourceCompile/Compiler.h>
#include <Surelog/Tests/Test.h>

#include <uhdm/Utils.h>
#include <uhdm/constant.h>
#include <uhdm/design.h>
#include <uhdm/int_typespec.h>
#include <uhdm/module.h>
#include <uhdm/net.h>
#include <uhdm/operation.h>
#include <uhdm/ref_typespec.h>
#include <uhdm/vpi_user.h>

namespace SURELOG {

class CastOp : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "6.24.1--cast_op.hlc"});

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

TEST_F(CastOp, ModuleExists) {
  ASSERT_NE(uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules()), nullptr);
}

// ---------------------------------------------------------------------------
// No processes — module-level `int a` is stored as a Net, not in a begin block
// ---------------------------------------------------------------------------
TEST_F(CastOp, NoProcesses) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getProcesses() == nullptr || top->getProcesses()->empty())
      << "int a = int'(...) at module level is stored as Net vpiValue, not in a process";
}

// ---------------------------------------------------------------------------
// Net "a" → IntTypespec (int keyword)
// ---------------------------------------------------------------------------
TEST_F(CastOp, NetAIsIntType) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const a = uhdm::findByName<uhdm::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr);
  EXPECT_NE(a->getTypespec()->getActual<uhdm::IntTypespec>(), nullptr)
      << "int keyword maps to IntTypespec (not IntegerTypespec)";
}

// ---------------------------------------------------------------------------
// Net "a" vpiValue = Operation(vpiCastOp=67)
// ---------------------------------------------------------------------------
TEST_F(CastOp, NetAValueIsCastOperation) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const a = uhdm::findByName<uhdm::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr);
  const uhdm::Operation *const castOp = a->getValue<uhdm::Operation>();
  ASSERT_NE(castOp, nullptr) << "int'(...) stored as Operation in Net's vpiValue";
  EXPECT_EQ(castOp->getOpType(), vpiCastOp);
}

TEST_F(CastOp, CastTypespecIsInt) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const a = uhdm::findByName<uhdm::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr);
  const uhdm::Operation *const castOp = a->getValue<uhdm::Operation>();
  ASSERT_NE(castOp, nullptr);
  const uhdm::RefTypespec *const rts = castOp->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<uhdm::IntTypespec>(), nullptr)
      << "int'(...) cast has IntTypespec as the cast target type";
}

TEST_F(CastOp, CastHasOneOperand) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const a = uhdm::findByName<uhdm::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr);
  const uhdm::Operation *const castOp = a->getValue<uhdm::Operation>();
  ASSERT_NE(castOp, nullptr);
  ASSERT_NE(castOp->getOperands(), nullptr);
  EXPECT_EQ(castOp->getOperands()->size(), 1u);
}

// ---------------------------------------------------------------------------
// Cast operand = Operation(vpiMultOp=25) — 2.1 * 3.7
// ---------------------------------------------------------------------------
TEST_F(CastOp, CastOperandIsMultiplyOperation) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const a = uhdm::findByName<uhdm::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr);
  const uhdm::Operation *const castOp = a->getValue<uhdm::Operation>();
  ASSERT_NE(castOp, nullptr);
  const uhdm::Operation *const multOp =
      any_cast<uhdm::Operation>(castOp->getOperands()->at(0));
  ASSERT_NE(multOp, nullptr);
  EXPECT_EQ(multOp->getOpType(), vpiMultOp);
}

TEST_F(CastOp, MultiplyOperandsAreRealConstants) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const a = uhdm::findByName<uhdm::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr);
  const uhdm::Operation *const castOp = a->getValue<uhdm::Operation>();
  ASSERT_NE(castOp, nullptr);
  const uhdm::Operation *const multOp =
      any_cast<uhdm::Operation>(castOp->getOperands()->at(0));
  ASSERT_NE(multOp, nullptr);
  ASSERT_NE(multOp->getOperands(), nullptr);
  ASSERT_EQ(multOp->getOperands()->size(), 2u);
  const uhdm::Constant *const lhs = any_cast<uhdm::Constant>(multOp->getOperands()->at(0));
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getConstType(), vpiRealConst);
  EXPECT_EQ(lhs->getDecompile(), "2.1");
  const uhdm::Constant *const rhs = any_cast<uhdm::Constant>(multOp->getOperands()->at(1));
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getConstType(), vpiRealConst);
  EXPECT_EQ(rhs->getDecompile(), "3.7");
}

// ---------------------------------------------------------------------------
// Structural completeness
// ---------------------------------------------------------------------------
TEST_F(CastOp, OneNetExists) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 1u) << "expected exactly 1 net: 'a'";
}

TEST_F(CastOp, NoContAssigns) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty())
      << "int a = int'(...) stores the cast as vpiValue, not a ContAssign";
}

}  // namespace SURELOG

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
