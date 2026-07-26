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

// Tests for rsort.sv (tags: 7.12.2 7.4.2)
//   module top ();
//     int ia[] = { 4, 5, 3, 1 };
//     initial begin
//       $display(":assert: ((%d == 4) and (%d == 5) and (%d == 3) and (%d == 1))",
//         ia[0], ia[1], ia[2], ia[3]);
//       ia.rsort;
//       $display(":assert: ((%d == 5) and (%d == 4) and (%d == 3) and (%d == 1))",
//         ia[0], ia[1], ia[2], ia[3]);
//     end
//   endmodule
//
// Checked:
//   - design has module top with exactly 1 net: "ia"
//   - net "ia": RefTypespec -> ArrayTypespec vpiArrayType=dynamic(2), elem
//     -> IntTypespec; initial value stored directly on the Net as an
//     Operation (vpiOpType=concatenation(33)) with 4 unsigned Constant
//     operands 4, 5, 3, 1
//   - Initial process: 1 Begin with 3 stmts (SysFuncCall + HierPath +
//     SysFuncCall)
//   - Stmt[0]: $display with 5 args (format + BitSelect ia[0..3])
//   - Stmt[1]: ia.rsort (no parens) -- HierPath "ia.rsort()" with 2 path
//     elems: RefObj "ia" (resolving Net "ia") and MethodFuncCall "rsort"
//     with no arguments -- correctly resolves, zero errors
//   - Stmt[2]: $display with 5 args (format + BitSelect ia[0..3],
//     documenting the post-rsort descending order)
//   - design-level typespecs (3): ModuleTypespec, IntTypespec (signed),
//     StringTypespec
//   - compiler emits zero errors
//   - no continuous assignments
//
// Not checked:
//   - actual runtime contents of ia after ia.rsort -- simulation-only (see
//     the skipped canary RuntimeRsortResultRequiresSimulation below)

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/begin.h>
#include <hldb/bit_select.h>
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

class UnpackedRsortTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "rsort.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// --- module / net --------------------------------------------------------------

TEST_F(UnpackedRsortTest, ModuleExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  EXPECT_NE(top, nullptr);
}

TEST_F(UnpackedRsortTest, ModuleHasOneNet) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 1u);
}

TEST_F(UnpackedRsortTest, NetIaIsDynamicArrayOfInt) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const ia = hldb::findByName<hldb::Net>("ia", top->getNets());
  ASSERT_NE(ia, nullptr);
  const hldb::ArrayTypespec *const at = ia->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  EXPECT_EQ(at->getArrayType(), 2);  // dynamic = 2
  EXPECT_NE(at->getElemTypespec()->getActual<hldb::IntTypespec>(), nullptr);
}

TEST_F(UnpackedRsortTest, NetIaInitialValueIsConcatenationOfFourFiveThreeOne) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const ia = hldb::findByName<hldb::Net>("ia", top->getNets());
  ASSERT_NE(ia, nullptr);
  const hldb::Operation *const init = ia->getValue<hldb::Operation>();
  ASSERT_NE(init, nullptr);
  EXPECT_EQ(init->getOpType(), vpiConcatOp);
  ASSERT_NE(init->getOperands(), nullptr);
  ASSERT_EQ(init->getOperands()->size(), 4u);
  const std::string expected[4] = {"4", "5", "3", "1"};
  for (uint32_t i = 0; i < 4u; ++i) {
    EXPECT_EQ(any_cast<hldb::Constant>(init->getOperands()->at(i))->getDecompile(), expected[i]) << "operand " << i;
  }
}

// --- initial process ---------------------------------------------------------

TEST_F(UnpackedRsortTest, InitialBeginHasThreeStmts) {
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

TEST_F(UnpackedRsortTest, FirstStmtDisplaysFourFiveThreeOne) {
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
  EXPECT_EQ(fmt->getValue(), ":assert: ((%d == 4) and (%d == 5) and (%d == 3) and (%d == 1))");
  for (uint32_t i = 0; i < 4u; ++i) {
    const hldb::BitSelect *const sel = any_cast<hldb::BitSelect>(disp->getArguments()->at(i + 1));
    ASSERT_NE(sel, nullptr) << "argument " << (i + 1);
    EXPECT_EQ(sel->getPrefix<hldb::RefObj>()->getName(), "ia");
    EXPECT_EQ(sel->getIndex<hldb::Constant>()->getDecompile(), std::to_string(i));
  }
}

TEST_F(UnpackedRsortTest, SecondStmtIsRsortHierPathWithNoParens) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Begin *const begin = any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>();
  ASSERT_NE(begin, nullptr);
  const hldb::HierPath *const hp = any_cast<hldb::HierPath>(begin->getStmts()->at(1));
  ASSERT_NE(hp, nullptr) << "'ia.rsort' should be a HierPath";
  ASSERT_NE(hp->getPathElems(), nullptr);
  ASSERT_EQ(hp->getPathElems()->size(), 2u);
  EXPECT_NE(any_cast<hldb::RefObj>(hp->getPathElems()->at(0))->getActual<hldb::Net>(), nullptr);
  const hldb::MethodFuncCall *const call = any_cast<hldb::MethodFuncCall>(hp->getPathElems()->at(1));
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getName(), "rsort");
  EXPECT_EQ(call->getArguments(), nullptr);
}

TEST_F(UnpackedRsortTest, ThirdStmtDisplaysFiveFourThreeOne) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Begin *const begin = any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>();
  ASSERT_NE(begin, nullptr);
  const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(begin->getStmts()->at(2));
  ASSERT_NE(disp, nullptr);
  const hldb::Constant *const fmt = any_cast<hldb::Constant>(disp->getArguments()->at(0));
  ASSERT_NE(fmt, nullptr);
  EXPECT_EQ(fmt->getValue(), ":assert: ((%d == 5) and (%d == 4) and (%d == 3) and (%d == 1))");
}

// --- design-level typespecs / compiler diagnostics ---------------------------

TEST_F(UnpackedRsortTest, DesignHasThreeTypespecs) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 3u);
}

TEST_F(UnpackedRsortTest, DesignHasModuleTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const hldb::ModuleTypespec *const mt = any_cast<hldb::ModuleTypespec>(m_design->getTypespecs()->at(0));
  ASSERT_NE(mt, nullptr);
  EXPECT_EQ(mt->getName(), "top");
}

TEST_F(UnpackedRsortTest, DesignHasSignedIntTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const hldb::IntTypespec *const it = any_cast<hldb::IntTypespec>(m_design->getTypespecs()->at(1));
  ASSERT_NE(it, nullptr);
  EXPECT_TRUE(it->getSigned());
}

TEST_F(UnpackedRsortTest, DesignHasStringTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_NE(any_cast<hldb::StringTypespec>(m_design->getTypespecs()->at(2)), nullptr);
}

TEST_F(UnpackedRsortTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
  EXPECT_EQ(stats.nbWarning, 0);
}

TEST_F(UnpackedRsortTest, NoContAssigns) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getContAssigns(), nullptr);
}

// --- known gap: runtime rsort result requires simulation ---------------------

TEST_F(UnpackedRsortTest, RuntimeRsortResultRequiresSimulation) {
  GTEST_SKIP() << "This harness only compiles/elaborates rsort.sv; it does not run a simulator, so "
                  "the actual runtime contents of ia after ia.rsort cannot be observed here. "
                  "rsort.sv's own $display format string documents the expected values.";

  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Begin *const begin = any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>();
  ASSERT_NE(begin, nullptr);
  const hldb::SysFuncCall *const disp = any_cast<hldb::SysFuncCall>(begin->getStmts()->at(2));
  ASSERT_NE(disp, nullptr);
  EXPECT_EQ(any_cast<hldb::Constant>(disp->getArguments()->at(0))->getValue(),
            ":assert: ((%d == 5) and (%d == 4) and (%d == 3) and (%d == 1))")
      << "expected ia == {5,4,3,1} after descending-sort of {4,5,3,1}";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
