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
//   - design has module top
//   - module has exactly 1 net: 'a' (IntTypespec)
//   - 'a' vpiValue = vpiCastOp Operation; cast typespec → IntTypespec
//   - cast has 1 operand: vpiMultOp(vpiRealConst "2.1", vpiRealConst "3.7")
//   - top has no continuous assignments (inline init stored as vpiValue, not ContAssign)
//   - top has no processes
//   - net 'a' vpiValue is not folded to a Constant at compile time (result is
//     only known at runtime)

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/int_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/ref_typespec.h>
#include <hldb/vpi_user.h>

namespace hlc {

class CastOp : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.24.1--cast_op.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(CastOp, ModuleExists) {
  ASSERT_NE(hldb::findByName<hldb::Module>("top", m_design->getAllModules()), nullptr);
}

// ---------------------------------------------------------------------------
// No processes — module-level `int a` is stored as a Net, not in a begin block
// ---------------------------------------------------------------------------
TEST_F(CastOp, NoProcesses) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getProcesses() == nullptr || top->getProcesses()->empty())
      << "int a = int'(...) at module level is stored as Net vpiValue, not in a process";
}

// ---------------------------------------------------------------------------
// Net "a" → IntTypespec (int keyword)
// ---------------------------------------------------------------------------
TEST_F(CastOp, NetAIsIntType) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const a = hldb::findByName<hldb::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr);
  EXPECT_NE(a->getTypespec()->getActual<hldb::IntTypespec>(), nullptr)
      << "int keyword maps to IntTypespec (not IntegerTypespec)";
}

// ---------------------------------------------------------------------------
// Net "a" vpiValue = Operation(vpiCastOp=67)
// ---------------------------------------------------------------------------
TEST_F(CastOp, NetAValueIsCastOperation) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const a = hldb::findByName<hldb::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr);
  const hldb::Operation *const castOp = a->getValue<hldb::Operation>();
  ASSERT_NE(castOp, nullptr) << "int'(...) stored as Operation in Net's vpiValue";
  EXPECT_EQ(castOp->getOpType(), vpiCastOp);
}

TEST_F(CastOp, CastTypespecIsInt) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const a = hldb::findByName<hldb::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr);
  const hldb::Operation *const castOp = a->getValue<hldb::Operation>();
  ASSERT_NE(castOp, nullptr);
  const hldb::RefTypespec *const rts = castOp->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::IntTypespec>(), nullptr) << "int'(...) cast has IntTypespec as the cast target type";
}

TEST_F(CastOp, CastHasOneOperand) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const a = hldb::findByName<hldb::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr);
  const hldb::Operation *const castOp = a->getValue<hldb::Operation>();
  ASSERT_NE(castOp, nullptr);
  ASSERT_NE(castOp->getOperands(), nullptr);
  EXPECT_EQ(castOp->getOperands()->size(), 1u);
}

// ---------------------------------------------------------------------------
// Cast operand = Operation(vpiMultOp=25) — 2.1 * 3.7
// ---------------------------------------------------------------------------
TEST_F(CastOp, CastOperandIsMultiplyOperation) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const a = hldb::findByName<hldb::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr);
  const hldb::Operation *const castOp = a->getValue<hldb::Operation>();
  ASSERT_NE(castOp, nullptr);
  const hldb::Operation *const multOp = any_cast<hldb::Operation>(castOp->getOperands()->at(0));
  ASSERT_NE(multOp, nullptr);
  EXPECT_EQ(multOp->getOpType(), vpiMultOp);
}

TEST_F(CastOp, MultiplyOperandsAreRealConstants) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const a = hldb::findByName<hldb::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr);
  const hldb::Operation *const castOp = a->getValue<hldb::Operation>();
  ASSERT_NE(castOp, nullptr);
  const hldb::Operation *const multOp = any_cast<hldb::Operation>(castOp->getOperands()->at(0));
  ASSERT_NE(multOp, nullptr);
  ASSERT_NE(multOp->getOperands(), nullptr);
  ASSERT_EQ(multOp->getOperands()->size(), 2u);
  const hldb::Constant *const lhs = any_cast<hldb::Constant>(multOp->getOperands()->at(0));
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getConstType(), vpiRealConst);
  EXPECT_EQ(lhs->getDecompile(), "2.1");
  const hldb::Constant *const rhs = any_cast<hldb::Constant>(multOp->getOperands()->at(1));
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getConstType(), vpiRealConst);
  EXPECT_EQ(rhs->getDecompile(), "3.7");
}

// ---------------------------------------------------------------------------
// Structural completeness
// ---------------------------------------------------------------------------
TEST_F(CastOp, OneNetExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 1u) << "expected exactly 1 net: 'a'";
}

TEST_F(CastOp, NoContAssigns) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty())
      << "int a = int'(...) stores the cast as vpiValue, not a ContAssign";
}

TEST_F(CastOp, NetAValueIsNotFoldedConstant) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const a = hldb::findByName<hldb::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(a->getValue<hldb::Constant>(), nullptr)
      << "int'(2.1 * 3.7) is stored as an Operation, not pre-evaluated to a Constant at compile time";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
