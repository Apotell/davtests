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

// Tests for multi.sv (tags: 7.4.5)
//   module top ();
//     // Same packed dimensions
//     bit [7:0] [31:0] arr_a [1:5] [1:10], arr_b [0:255];
//   endmodule
//
// Checked:
//   - design has module top with exactly 2 variables: "arr_a", "arr_b"
//     declared in a single multi-variable Data_declaration that shares the
//     same packed dimensions [7:0][31:0] but has DIFFERENT unpacked
//     dimensions per variable
//   - variable "arr_a": 2 unpacked dims [1:5][1:10] -- outer ArrayTypespec
//     static(1) range [1:5] whose elem is ANOTHER ArrayTypespec static(1)
//     range [1:10] whose elem is BitTypespec with 2 packed ranges
//     [7:0][31:0]
//   - variable "arr_b": 1 unpacked dim [0:255] -- ArrayTypespec static(1) range
//     [0:255] whose elem is BitTypespec with the SAME 2 packed ranges
//     [7:0][31:0] as arr_a's element type, and IS the same shared BitTypespec
//     instance as arr_a's element type (deduplicated by
//     hldb::BuiltInTypespecsPruner)
//   - module has exactly 4 typespecs: 1 BitTypespec (shared packed-dim
//     definition, reused by both arr_a and arr_b's element chains per the
//     comment "Same packed dimensions") + 3 ArrayTypespec (arr_a's 2
//     unpacked levels + arr_b's 1 unpacked level)
//   - design-level typespecs (2): ModuleTypespec, IntTypespec (signed) -- no
//     StringTypespec since there is no initial block / $display
//   - module has no processes (pure declaration, no initial/always block)
//   - no continuous assignments
//   - compiler emits zero errors
//
// Not checked:
//   - none -- multi.sv is declaration-only with no runtime behavior to
//     defer to simulation

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/bit_typespec.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/int_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/variable.h>
#include <hldb/range.h>
#include <hldb/ref_typespec.h>
#include <hldb/vpi_user.h>

namespace hlc {

class MultiDimMultiDeclarationTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "multi.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// --- module / variables ----

TEST_F(MultiDimMultiDeclarationTest, ModuleExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  EXPECT_NE(top, nullptr);
}

TEST_F(MultiDimMultiDeclarationTest, ModuleHasTwoVariables) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_EQ(top->getVariables()->size(), 2u);
}

TEST_F(MultiDimMultiDeclarationTest, ModuleHasFourTypespecs) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTypespecs(), nullptr);
  EXPECT_EQ(top->getTypespecs()->size(), 4u);
}

TEST_F(MultiDimMultiDeclarationTest, VariableArrANameIsArrA) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const arrA = hldb::findByName<hldb::Variable>("arr_a", top->getVariables());
  ASSERT_NE(arrA, nullptr);
  EXPECT_EQ(arrA->getName(), "arr_a");
}

TEST_F(MultiDimMultiDeclarationTest, VariableArrBNameIsArrB) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const arrB = hldb::findByName<hldb::Variable>("arr_b", top->getVariables());
  ASSERT_NE(arrB, nullptr);
  EXPECT_EQ(arrB->getName(), "arr_b");
}

// --- variable arr_a: [7:0][31:0] arr_a [1:5][1:10] ----

TEST_F(MultiDimMultiDeclarationTest, VariableArrAOuterDimIsOneToFive) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const arrA = hldb::findByName<hldb::Variable>("arr_a", top->getVariables());
  ASSERT_NE(arrA, nullptr);
  const hldb::ArrayTypespec *const outer = arrA->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(outer, nullptr);
  EXPECT_EQ(outer->getArrayType(), 1);  // static = 1
  ASSERT_NE(outer->getRange(), nullptr);
  EXPECT_EQ(outer->getRange()->getLeftExpr<hldb::Constant>()->getDecompile(), "1");
  EXPECT_EQ(outer->getRange()->getRightExpr<hldb::Constant>()->getDecompile(), "5");
  EXPECT_NE(outer->getElemTypespec()->getActual<hldb::ArrayTypespec>(), nullptr)
      << "arr_a's outer dim elem should itself be another ArrayTypespec, not the BitTypespec directly";
}

TEST_F(MultiDimMultiDeclarationTest, VariableArrAInnerDimIsOneToTen) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const arrA = hldb::findByName<hldb::Variable>("arr_a", top->getVariables());
  ASSERT_NE(arrA, nullptr);
  const hldb::ArrayTypespec *const outer = arrA->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(outer, nullptr);
  const hldb::ArrayTypespec *const inner = outer->getElemTypespec()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(inner, nullptr);
  EXPECT_EQ(inner->getArrayType(), 1);  // static = 1
  ASSERT_NE(inner->getRange(), nullptr);
  EXPECT_EQ(inner->getRange()->getLeftExpr<hldb::Constant>()->getDecompile(), "1");
  EXPECT_EQ(inner->getRange()->getRightExpr<hldb::Constant>()->getDecompile(), "10");
}

TEST_F(MultiDimMultiDeclarationTest, VariableArrAElemIsBitTypespecWithPackedRanges7And31) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const arrA = hldb::findByName<hldb::Variable>("arr_a", top->getVariables());
  ASSERT_NE(arrA, nullptr);
  const hldb::ArrayTypespec *const outer = arrA->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(outer, nullptr);
  const hldb::ArrayTypespec *const inner = outer->getElemTypespec()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(inner, nullptr);
  const hldb::BitTypespec *const bt = inner->getElemTypespec()->getActual<hldb::BitTypespec>();
  ASSERT_NE(bt, nullptr);
  ASSERT_NE(bt->getRanges(), nullptr);
  ASSERT_EQ(bt->getRanges()->size(), 2u);
  EXPECT_EQ(bt->getRanges()->at(0)->getLeftExpr<hldb::Constant>()->getDecompile(), "7");
  EXPECT_EQ(bt->getRanges()->at(0)->getRightExpr<hldb::Constant>()->getDecompile(), "0");
  EXPECT_EQ(bt->getRanges()->at(1)->getLeftExpr<hldb::Constant>()->getDecompile(), "31");
  EXPECT_EQ(bt->getRanges()->at(1)->getRightExpr<hldb::Constant>()->getDecompile(), "0");
}

// --- variable arr_b: [7:0][31:0] arr_b [0:255] ----

TEST_F(MultiDimMultiDeclarationTest, VariableArrBDimIsZeroToTwoFiveFive) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const arrB = hldb::findByName<hldb::Variable>("arr_b", top->getVariables());
  ASSERT_NE(arrB, nullptr);
  const hldb::ArrayTypespec *const at = arrB->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  EXPECT_EQ(at->getArrayType(), 1);  // static = 1
  ASSERT_NE(at->getRange(), nullptr);
  EXPECT_EQ(at->getRange()->getLeftExpr<hldb::Constant>()->getDecompile(), "0");
  EXPECT_EQ(at->getRange()->getRightExpr<hldb::Constant>()->getDecompile(), "255");
}

TEST_F(MultiDimMultiDeclarationTest, VariableArrBElemIsBitTypespecWithPackedRanges7And31) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const arrB = hldb::findByName<hldb::Variable>("arr_b", top->getVariables());
  ASSERT_NE(arrB, nullptr);
  const hldb::ArrayTypespec *const at = arrB->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  const hldb::BitTypespec *const bt = at->getElemTypespec()->getActual<hldb::BitTypespec>();
  ASSERT_NE(bt, nullptr);
  ASSERT_NE(bt->getRanges(), nullptr);
  ASSERT_EQ(bt->getRanges()->size(), 2u);
  EXPECT_EQ(bt->getRanges()->at(0)->getLeftExpr<hldb::Constant>()->getDecompile(), "7");
  EXPECT_EQ(bt->getRanges()->at(1)->getLeftExpr<hldb::Constant>()->getDecompile(), "31");
}

TEST_F(MultiDimMultiDeclarationTest, ArrAAndArrBElemTypespecsAreSharedInstance) {
  // "Same packed dimensions" (per the source comment) means textually
  // identical -- IEEE 1800-2023 has no notion of type-descriptor object
  // identity (only value/structural equivalence, Sec 6.22), so this is a
  // tool-modeling choice, not a standard requirement. HLC deliberately
  // merges structurally-identical builtin typespecs post-elaboration via
  // hldb::BuiltInTypespecsPruner (third_party/hldb/src/Pruner.cpp) -- its
  // BuiltInTypespecsCollector walks BitTypespecCollection/
  // ArrayTypespecCollection (among others) and swaps in the first duplicate
  // found, ignoring source-location fields for builtin typespecs. So
  // arr_a's and arr_b's identical [7:0][31:0] BitTypespec end up as the
  // SAME shared instance, matching ModuleHasFourTypespecs's count of 4
  // above (1 BitTypespec, not 2).
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const arrA = hldb::findByName<hldb::Variable>("arr_a", top->getVariables());
  const hldb::Variable *const arrB = hldb::findByName<hldb::Variable>("arr_b", top->getVariables());
  ASSERT_NE(arrA, nullptr);
  ASSERT_NE(arrB, nullptr);
  const hldb::ArrayTypespec *const outerA = arrA->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  const hldb::ArrayTypespec *const innerA = outerA->getElemTypespec()->getActual<hldb::ArrayTypespec>();
  const hldb::BitTypespec *const btA = innerA->getElemTypespec()->getActual<hldb::BitTypespec>();
  const hldb::ArrayTypespec *const atB = arrB->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  const hldb::BitTypespec *const btB = atB->getElemTypespec()->getActual<hldb::BitTypespec>();
  ASSERT_NE(btA, nullptr);
  ASSERT_NE(btB, nullptr);
  EXPECT_EQ(btA, btB);
}

// --- design-level typespecs / structural completeness ----

TEST_F(MultiDimMultiDeclarationTest, DesignHasTwoTypespecs) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 2u);
}

TEST_F(MultiDimMultiDeclarationTest, DesignHasModuleTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const hldb::ModuleTypespec *const mt = any_cast<hldb::ModuleTypespec>(m_design->getTypespecs()->at(0));
  ASSERT_NE(mt, nullptr);
  EXPECT_EQ(mt->getName(), "top");
}

TEST_F(MultiDimMultiDeclarationTest, DesignHasSignedIntTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const hldb::IntTypespec *const it = any_cast<hldb::IntTypespec>(m_design->getTypespecs()->at(1));
  ASSERT_NE(it, nullptr);
  EXPECT_TRUE(it->getSigned());
}

TEST_F(MultiDimMultiDeclarationTest, NoProcesses) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getProcesses(), nullptr);
}

TEST_F(MultiDimMultiDeclarationTest, NoContAssigns) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getContAssigns(), nullptr);
}

TEST_F(MultiDimMultiDeclarationTest, CompilerReportsZeroErrors) {
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
