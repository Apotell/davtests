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

// Tests for product.sv (tags: 7.12.3 7.4.2)
//   module top ();
//     byte b[] = { 1, 2, 3, 4 };
//     int y;
//     initial begin
//       $display(":assert: ((%d == 1) and (%d == 2) and (%d == 3) and (%d == 4))",
//         b[0], b[1], b[2], b[3]);
//       y = b.product;
//       $display(":assert: (%d == 24)", y);
//     end
//   endmodule
//
// Checked:
//   - design has module top with exactly 2 nets: "b", "y"
//   - net "b": RefTypespec -> ArrayTypespec vpiArrayType=dynamic(2), elem
//     -> ByteTypespec (signed); initial value stored directly on the Net
//     as an Operation (vpiOpType=concatenation(33)) with 4 unsigned
//     Constant operands 1, 2, 3, 4
//   - net "y": RefTypespec -> IntTypespec (plain scalar, not an array),
//     with no initial value
//   - Initial process: 1 Begin with 3 stmts (SysFuncCall + Assignment +
//     SysFuncCall)
//   - Stmt[0]: $display with 5 args (format + BitSelect b[0..3])
//   - Stmt[1]: y = b.product (no parens) -- blocking Assignment, lhs
//     RefObj "y" resolving Net "y", rhs HierPath "b.product()" with 2 path
//     elems: RefObj "b" (resolving Net "b") and MethodFuncCall "product"
//     with no arguments -- correctly resolves, zero errors
//   - Stmt[2]: $display with 2 args (format + RefObj "y")
//   - design-level typespecs (3): ModuleTypespec, IntTypespec (signed),
//     StringTypespec
//   - compiler emits zero errors
//   - no continuous assignments
//
// Not checked:
//   - actual runtime value of y after y = b.product -- simulation-only
//     (see the skipped canary RuntimeProductResultRequiresSimulation
//     below)

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/bit_select.h>
#include <hldb/byte_typespec.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/hier_path.h>
#include <hldb/initial.h>
#include <hldb/int_typespec.h>
#include <hldb/method_func_call.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/string_typespec.h>
#include <hldb/sys_func_call.h>
#include <hldb/vpi_user.h>

namespace hlc {

class UnpackedProductTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "product.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// --- module / nets ------------------------------------------------------------

TEST_F(UnpackedProductTest, ModuleExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  EXPECT_NE(top, nullptr);
}

TEST_F(UnpackedProductTest, ModuleHasTwoNets) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 2u);
}

TEST_F(UnpackedProductTest, NetBIsDynamicArrayOfSignedByte) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const b = hldb::findByName<hldb::Net>("b", top->getNets());
  ASSERT_NE(b, nullptr);
  const hldb::ArrayTypespec *const at = b->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  EXPECT_EQ(at->getArrayType(), 2);  // dynamic = 2
  const hldb::ByteTypespec *const elem = at->getElemTypespec()->getActual<hldb::ByteTypespec>();
  ASSERT_NE(elem, nullptr);
  EXPECT_TRUE(elem->getSigned());
}

TEST_F(UnpackedProductTest, NetBInitialValueIsConcatenationOfOneTwoThreeFour) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const b = hldb::findByName<hldb::Net>("b", top->getNets());
  ASSERT_NE(b, nullptr);
  const hldb::Operation *const init = b->getValue<hldb::Operation>();
  ASSERT_NE(init, nullptr);
  EXPECT_EQ(init->getOpType(), vpiConcatOp);
  ASSERT_NE(init->getOperands(), nullptr);
  ASSERT_EQ(init->getOperands()->size(), 4u);
  for (uint32_t i = 0; i < 4u; ++i) {
    EXPECT_EQ(any_cast<hldb::Constant>(init->getOperands()->at(i))->getDecompile(), std::to_string(i + 1))
        << "operand " << i;
  }
}

TEST_F(UnpackedProductTest, NetYIsPlainSignedIntWithNoInitialValue) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const y = hldb::findByName<hldb::Net>("y", top->getNets());
  ASSERT_NE(y, nullptr);
  EXPECT_NE(y->getTypespec<hldb::RefTypespec>()->getActual<hldb::IntTypespec>(), nullptr);
  EXPECT_EQ(y->getValue(), nullptr);
}

// --- initial process ---------------------------------------------------------

TEST_F(UnpackedProductTest, InitialBeginHasThreeStmts) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const begin = init->getStmt<hldb::Begin>();
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  EXPECT_EQ(begin->getStmts()->size(), 3u);
}

TEST_F(UnpackedProductTest, FirstStmtDisplaysOneTwoThreeFour) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Begin *const begin = any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>();
  ASSERT_NE(begin, nullptr);
  const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(begin->getStmts()->at(0));
  ASSERT_NE(disp, nullptr);
  ASSERT_NE(disp->getArguments(), nullptr);
  ASSERT_EQ(disp->getArguments()->size(), 5u);
  const hldb::Constant *const fmt = any_cast<hldb::Constant>(disp->getArguments()->at(0));
  ASSERT_NE(fmt, nullptr);
  EXPECT_EQ(fmt->getValue(), ":assert: ((%d == 1) and (%d == 2) and (%d == 3) and (%d == 4))");
  for (uint32_t i = 0; i < 4u; ++i) {
    const hldb::BitSelect *const sel = any_cast<hldb::BitSelect>(disp->getArguments()->at(i + 1));
    ASSERT_NE(sel, nullptr) << "argument " << (i + 1);
    EXPECT_EQ(sel->getPrefix<hldb::RefObj>()->getName(), "b");
    EXPECT_EQ(sel->getIndex<hldb::Constant>()->getDecompile(), std::to_string(i));
  }
}

TEST_F(UnpackedProductTest, SecondStmtAssignsYFromBProductReduction) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Begin *const begin = any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>();
  ASSERT_NE(begin, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(begin->getStmts()->at(1));
  ASSERT_NE(assign, nullptr);
  EXPECT_TRUE(assign->getBlocking());
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "y");
  EXPECT_NE(lhs->getActual<hldb::Net>(), nullptr);
  const hldb::HierPath *const hp = assign->getRhs<hldb::HierPath>();
  ASSERT_NE(hp, nullptr);
  ASSERT_NE(hp->getPathElems(), nullptr);
  ASSERT_EQ(hp->getPathElems()->size(), 2u);
  EXPECT_NE(any_cast<hldb::RefObj>(hp->getPathElems()->at(0))->getActual<hldb::Net>(), nullptr);
  const hldb::MethodFuncCall *const call = any_cast<hldb::MethodFuncCall>(hp->getPathElems()->at(1));
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getName(), "product");
  EXPECT_EQ(call->getArguments(), nullptr);
}

TEST_F(UnpackedProductTest, ThirdStmtDisplaysYEqualsTwentyFour) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Begin *const begin = any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>();
  ASSERT_NE(begin, nullptr);
  const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(begin->getStmts()->at(2));
  ASSERT_NE(disp, nullptr);
  ASSERT_NE(disp->getArguments(), nullptr);
  ASSERT_EQ(disp->getArguments()->size(), 2u);
  const hldb::Constant *const fmt = any_cast<hldb::Constant>(disp->getArguments()->at(0));
  ASSERT_NE(fmt, nullptr);
  EXPECT_EQ(fmt->getValue(), ":assert: (%d == 24)");
  const hldb::RefObj *const yRef = any_cast<hldb::RefObj>(disp->getArguments()->at(1));
  ASSERT_NE(yRef, nullptr);
  EXPECT_EQ(yRef->getName(), "y");
  EXPECT_NE(yRef->getActual<hldb::Net>(), nullptr);
}

// --- design-level typespecs / compiler diagnostics ---------------------------

TEST_F(UnpackedProductTest, DesignHasThreeTypespecs) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 3u);
}

TEST_F(UnpackedProductTest, DesignHasModuleTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const hldb::ModuleTypespec *const mt = any_cast<hldb::ModuleTypespec>(m_design->getTypespecs()->at(0));
  ASSERT_NE(mt, nullptr);
  EXPECT_EQ(mt->getName(), "top");
}

TEST_F(UnpackedProductTest, DesignHasSignedIntTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const hldb::IntTypespec *const it = any_cast<hldb::IntTypespec>(m_design->getTypespecs()->at(1));
  ASSERT_NE(it, nullptr);
  EXPECT_TRUE(it->getSigned());
}

TEST_F(UnpackedProductTest, DesignHasStringTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_NE(any_cast<hldb::StringTypespec>(m_design->getTypespecs()->at(2)), nullptr);
}

TEST_F(UnpackedProductTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
  EXPECT_EQ(stats.nbWarning, 0);
}

TEST_F(UnpackedProductTest, NoContAssigns) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getContAssigns(), nullptr);
}

// --- known gap: runtime product-reduction result requires simulation -------

TEST_F(UnpackedProductTest, RuntimeProductResultRequiresSimulation) {
  GTEST_SKIP() << "This harness only compiles/elaborates product.sv; it does not run a simulator, "
                  "so the actual runtime value of y after y = b.product cannot be observed here. "
                  "product.sv's own $display format string documents the expected value.";

  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Begin *const begin = any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>();
  ASSERT_NE(begin, nullptr);
  const hldb::SysFuncCall *const disp = any_cast<hldb::SysFuncCall>(begin->getStmts()->at(2));
  ASSERT_NE(disp, nullptr);
  EXPECT_EQ(any_cast<hldb::Constant>(disp->getArguments()->at(0))->getValue(), ":assert: (%d == 24)")
      << "expected y == (1 * 2 * 3 * 4) == 24";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
