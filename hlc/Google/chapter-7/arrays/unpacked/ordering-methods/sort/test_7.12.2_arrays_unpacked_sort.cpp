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

// Tests for sort.sv (tags: 7.12.2 7.4.2)
//   module top ();
//     int ia[] = { 4, 5, 3, 1 };
//     initial begin
//       $display(":assert: ((%d == 4) and (%d == 5) and (%d == 3) and (%d == 1))",
//         ia[0], ia[1], ia[2], ia[3]);
//       ia.sort;
//       $display(":assert: ((%d == 1) and (%d == 3) and (%d == 4) and (%d == 5))",
//         ia[0], ia[1], ia[2], ia[3]);
//     end
//   endmodule
//
// Checked:
//   - design has module top with exactly 1 variable: "ia" (IEEE 1800-2023
//     6.7/6.8: 'int ia[] = {...}' has no net-type keyword, so it is a
//     variable_declaration, not a net_declaration); it does not appear in
//     getNets()
//   - variable "ia": RefTypespec -> ArrayTypespec vpiArrayType=dynamic(2),
//     elem -> IntTypespec; initial value stored directly on the Variable as
//     an Operation (vpiOpType=concatenation(33)) with 4 unsigned Constant
//     operands 4, 5, 3, 1
//   - Initial process: 1 Begin with 3 stmts (SysFuncCall + HierPath +
//     SysFuncCall)
//   - Stmt[0]: $display with 5 args (format + BitSelect ia[0..3])
//   - Stmt[1]: ia.sort (no parens) -- HierPath "ia.sort()" with 2 path
//     elems: RefObj "ia" (resolving Variable "ia") and MethodFuncCall "sort"
//     with no arguments -- correctly resolves, zero errors
//   - Stmt[2]: $display with 5 args (format + BitSelect ia[0..3],
//     documenting the post-sort ascending order)
//   - design-level typespecs (3): ModuleTypespec, IntTypespec (signed),
//     StringTypespec
//   - compiler emits zero errors
//   - no continuous assignments
//
// Not checked:
//   - actual runtime contents of ia after ia.sort -- simulation-only (see
//     the skipped canary RuntimeSortResultRequiresSimulation below)

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
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
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/string_typespec.h>
#include <hldb/sys_func_call.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class UnpackedSortTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "sort.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// --- module / variable ----

TEST_F(UnpackedSortTest, ModuleExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  EXPECT_NE(top, nullptr);
}

TEST_F(UnpackedSortTest, ModuleHasOneVariable) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_EQ(top->getVariables()->size(), 1u)
      << "6.7/6.8: 'int ia[] = {...}' declared with no net-type keyword is a variable";
}

TEST_F(UnpackedSortTest, ModuleHasNoNets) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getNets(), nullptr) << "no net-type keyword is present in sort.sv";
}

TEST_F(UnpackedSortTest, VarIaIsDynamicArrayOfInt) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const ia = hldb::findByName<hldb::Variable>("ia", top->getVariables());
  ASSERT_NE(ia, nullptr);
  const hldb::ArrayTypespec *const at = ia->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  EXPECT_EQ(at->getArrayType(), vpiDynamicArray);
  EXPECT_NE(at->getElemTypespec()->getActual<hldb::IntTypespec>(), nullptr);
}

TEST_F(UnpackedSortTest, VarIaInitialValueIsConcatenationOfFourFiveThreeOne) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const ia = hldb::findByName<hldb::Variable>("ia", top->getVariables());
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

// --- initial process ----

TEST_F(UnpackedSortTest, InitialBeginHasThreeStmts) {
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

TEST_F(UnpackedSortTest, FirstStmtDisplaysFourFiveThreeOne) {
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

TEST_F(UnpackedSortTest, SecondStmtIsSortHierPathWithNoParens) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Begin *const begin = any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>();
  ASSERT_NE(begin, nullptr);
  const hldb::HierPath *const hp = any_cast<hldb::HierPath>(begin->getStmts()->at(1));
  ASSERT_NE(hp, nullptr) << "'ia.sort' should be a HierPath";
  ASSERT_NE(hp->getPathElems(), nullptr);
  ASSERT_EQ(hp->getPathElems()->size(), 2u);
  EXPECT_NE(any_cast<hldb::RefObj>(hp->getPathElems()->at(0))->getActual<hldb::Variable>(), nullptr);
  const hldb::MethodFuncCall *const call = any_cast<hldb::MethodFuncCall>(hp->getPathElems()->at(1));
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getName(), "sort");
  EXPECT_EQ(call->getArguments(), nullptr);
}

TEST_F(UnpackedSortTest, ThirdStmtDisplaysOneThreeFourFive) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Begin *const begin = any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>();
  ASSERT_NE(begin, nullptr);
  const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(begin->getStmts()->at(2));
  ASSERT_NE(disp, nullptr);
  const hldb::Constant *const fmt = any_cast<hldb::Constant>(disp->getArguments()->at(0));
  ASSERT_NE(fmt, nullptr);
  EXPECT_EQ(fmt->getValue(), ":assert: ((%d == 1) and (%d == 3) and (%d == 4) and (%d == 5))");
}

// --- design-level typespecs / compiler diagnostics ----

TEST_F(UnpackedSortTest, DesignHasThreeTypespecs) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 3u);
}

TEST_F(UnpackedSortTest, DesignHasModuleTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const hldb::ModuleTypespec *const mt = any_cast<hldb::ModuleTypespec>(m_design->getTypespecs()->at(0));
  ASSERT_NE(mt, nullptr);
  EXPECT_EQ(mt->getName(), "top");
}

TEST_F(UnpackedSortTest, DesignHasSignedIntTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const hldb::IntTypespec *const it = any_cast<hldb::IntTypespec>(m_design->getTypespecs()->at(1));
  ASSERT_NE(it, nullptr);
  EXPECT_TRUE(it->getSigned());
}

TEST_F(UnpackedSortTest, DesignHasStringTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_NE(any_cast<hldb::StringTypespec>(m_design->getTypespecs()->at(2)), nullptr);
}

TEST_F(UnpackedSortTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "sort"), nullptr)
      << "arr.sort() must bind (IEEE 1800-2023 7.12.2)";
  EXPECT_EQ(stats.nbWarning, 0);
}

TEST_F(UnpackedSortTest, NoContAssigns) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getContAssigns(), nullptr);
}

// --- known gap: runtime sort result requires simulation ----

TEST_F(UnpackedSortTest, RuntimeSortResultRequiresSimulation) {
  GTEST_SKIP() << "This harness only compiles/elaborates sort.sv; it does not run a simulator, so "
                  "the actual runtime contents of ia after ia.sort cannot be observed here. sort.sv's "
                  "own $display format string documents the expected values.";

  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Begin *const begin = any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>();
  ASSERT_NE(begin, nullptr);
  const hldb::SysFuncCall *const disp = any_cast<hldb::SysFuncCall>(begin->getStmts()->at(2));
  ASSERT_NE(disp, nullptr);
  EXPECT_EQ(any_cast<hldb::Constant>(disp->getArguments()->at(0))->getValue(),
            ":assert: ((%d == 1) and (%d == 3) and (%d == 4) and (%d == 5))")
      << "expected ia == {1,3,4,5} after ascending-sort of {4,5,3,1}";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
