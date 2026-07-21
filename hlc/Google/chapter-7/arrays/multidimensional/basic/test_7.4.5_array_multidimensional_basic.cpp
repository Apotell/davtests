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

// Tests for basic.sv (tags: 7.4.5)
//   module top ();
//     // 10 elements of 4 8-bit bytes (each element packed into 32 bits)
//     bit [3:0] [7:0] arr [1:10];
//     // compatible with memory array
//     bit [7:0] mem [0:255];
//     // Varies most rapidly: 1 to 6 / 1 to 5 / 1 to 8 / 1 to 7
//     bit [1:5] [1:6] arr2 [1:7] [1:8];
//   endmodule
//
// Checked:
//   - design has module work@top with exactly 3 nets: "arr", "mem", "arr2"
//   - net "arr": ArrayTypespec vpiArrayType=static(1), range [1:10], elem ->
//     BitTypespec with 2 packed ranges [3:0][7:0] (vector=true)
//   - net "mem": ArrayTypespec static(1), range [0:255], elem -> BitTypespec
//     with 1 packed range [7:0]
//   - net "arr2": nested unpacked dims -- outer ArrayTypespec static(1)
//     range [1:7] whose elem is ANOTHER ArrayTypespec static(1) range [1:8]
//     whose elem is BitTypespec with 2 packed ranges [1:5][1:6] -- i.e. the
//     leftmost unpacked dimension [1:7] is outermost, [1:8] is the dimension
//     closest to the declared name, matching IEEE 1800-2017 7.4.5's
//     "varies most rapidly" ordering
//   - module has exactly 7 typespecs (3 BitTypespec + 4 ArrayTypespec, one
//     pair per net plus an extra ArrayTypespec level for arr2's 2 unpacked
//     dims)
//   - design-level typespecs (2): ModuleTypespec, IntTypespec (signed) --
//     no StringTypespec since there is no initial block / $display
//   - module has no processes (pure declarations, no initial/always block)
//   - no continuous assignments
//   - compiler emits zero errors
//
// Not checked:
//   - none -- basic.sv is declarations-only with no runtime behavior to
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
#include <hldb/net.h>
#include <hldb/range.h>
#include <hldb/ref_typespec.h>
#include <hldb/vpi_user.h>

namespace hlc {

class MultiDimBasicTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "basic.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// --- module -------------------------------------------------------------------

TEST_F(MultiDimBasicTest, ModuleExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  EXPECT_NE(top, nullptr);
}

TEST_F(MultiDimBasicTest, ModuleHasThreeNets) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 3u);
}

TEST_F(MultiDimBasicTest, ModuleHasSevenTypespecs) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTypespecs(), nullptr);
  EXPECT_EQ(top->getTypespecs()->size(), 7u);
}

// --- net arr: bit [3:0][7:0] arr [1:10] ---------------------------------------

TEST_F(MultiDimBasicTest, NetArrNameAndFullName) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const arr = hldb::findByName<hldb::Net>("arr", top->getNets());
  ASSERT_NE(arr, nullptr);
  EXPECT_EQ(arr->getFullName(), "work@top.arr");
}

TEST_F(MultiDimBasicTest, NetArrIsStaticArrayRangeOneToTen) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const arr = hldb::findByName<hldb::Net>("arr", top->getNets());
  ASSERT_NE(arr, nullptr);
  const hldb::ArrayTypespec *const at = arr->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  EXPECT_EQ(at->getArrayType(), 1);  // static = 1
  ASSERT_NE(at->getRange(), nullptr);
  const hldb::Constant *const left = at->getRange()->getLeftExpr<hldb::Constant>();
  const hldb::Constant *const right = at->getRange()->getRightExpr<hldb::Constant>();
  ASSERT_NE(left, nullptr);
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(left->getDecompile(), "1");
  EXPECT_EQ(right->getDecompile(), "10");
}

TEST_F(MultiDimBasicTest, NetArrElemIsBitTypespecWithTwoPackedRanges) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const arr = hldb::findByName<hldb::Net>("arr", top->getNets());
  ASSERT_NE(arr, nullptr);
  const hldb::ArrayTypespec *const at = arr->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getElemTypespec(), nullptr);
  const hldb::BitTypespec *const bt = at->getElemTypespec()->getActual<hldb::BitTypespec>();
  ASSERT_NE(bt, nullptr);
  EXPECT_TRUE(bt->getVector());
  ASSERT_NE(bt->getRanges(), nullptr);
  ASSERT_EQ(bt->getRanges()->size(), 2u);
  EXPECT_EQ(bt->getRanges()->at(0)->getLeftExpr<hldb::Constant>()->getDecompile(), "3");
  EXPECT_EQ(bt->getRanges()->at(0)->getRightExpr<hldb::Constant>()->getDecompile(), "0");
  EXPECT_EQ(bt->getRanges()->at(1)->getLeftExpr<hldb::Constant>()->getDecompile(), "7");
  EXPECT_EQ(bt->getRanges()->at(1)->getRightExpr<hldb::Constant>()->getDecompile(), "0");
}

// --- net mem: bit [7:0] mem [0:255] -------------------------------------------

TEST_F(MultiDimBasicTest, NetMemIsStaticArrayRangeZeroToTwoFiveFive) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const mem = hldb::findByName<hldb::Net>("mem", top->getNets());
  ASSERT_NE(mem, nullptr);
  const hldb::ArrayTypespec *const at = mem->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  EXPECT_EQ(at->getArrayType(), 1);  // static = 1
  ASSERT_NE(at->getRange(), nullptr);
  EXPECT_EQ(at->getRange()->getLeftExpr<hldb::Constant>()->getDecompile(), "0");
  EXPECT_EQ(at->getRange()->getRightExpr<hldb::Constant>()->getDecompile(), "255");
}

TEST_F(MultiDimBasicTest, NetMemElemIsBitTypespecWithOnePackedRange) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const mem = hldb::findByName<hldb::Net>("mem", top->getNets());
  ASSERT_NE(mem, nullptr);
  const hldb::ArrayTypespec *const at = mem->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  const hldb::BitTypespec *const bt = at->getElemTypespec()->getActual<hldb::BitTypespec>();
  ASSERT_NE(bt, nullptr);
  ASSERT_NE(bt->getRanges(), nullptr);
  ASSERT_EQ(bt->getRanges()->size(), 1u);
  EXPECT_EQ(bt->getRanges()->at(0)->getLeftExpr<hldb::Constant>()->getDecompile(), "7");
  EXPECT_EQ(bt->getRanges()->at(0)->getRightExpr<hldb::Constant>()->getDecompile(), "0");
}

// --- net arr2: bit [1:5][1:6] arr2 [1:7][1:8] ---------------------------------

TEST_F(MultiDimBasicTest, NetArr2OuterDimIsOneToSeven) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const arr2 = hldb::findByName<hldb::Net>("arr2", top->getNets());
  ASSERT_NE(arr2, nullptr);
  const hldb::ArrayTypespec *const outer = arr2->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(outer, nullptr);
  EXPECT_EQ(outer->getArrayType(), 1);  // static = 1
  ASSERT_NE(outer->getRange(), nullptr);
  EXPECT_EQ(outer->getRange()->getLeftExpr<hldb::Constant>()->getDecompile(), "1");
  EXPECT_EQ(outer->getRange()->getRightExpr<hldb::Constant>()->getDecompile(), "7");
}

TEST_F(MultiDimBasicTest, NetArr2InnerDimIsOneToEight) {
  // arr2 [1:7] [1:8] -- [1:8] is the dimension closest to the name, i.e. the
  // element type of the outer [1:7] ArrayTypespec is itself an ArrayTypespec.
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const arr2 = hldb::findByName<hldb::Net>("arr2", top->getNets());
  ASSERT_NE(arr2, nullptr);
  const hldb::ArrayTypespec *const outer = arr2->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(outer, nullptr);
  ASSERT_NE(outer->getElemTypespec(), nullptr);
  const hldb::ArrayTypespec *const inner = outer->getElemTypespec()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(inner, nullptr);
  EXPECT_EQ(inner->getArrayType(), 1);  // static = 1
  ASSERT_NE(inner->getRange(), nullptr);
  EXPECT_EQ(inner->getRange()->getLeftExpr<hldb::Constant>()->getDecompile(), "1");
  EXPECT_EQ(inner->getRange()->getRightExpr<hldb::Constant>()->getDecompile(), "8");
}

TEST_F(MultiDimBasicTest, NetArr2ElemIsBitTypespecWithTwoPackedRanges) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const arr2 = hldb::findByName<hldb::Net>("arr2", top->getNets());
  ASSERT_NE(arr2, nullptr);
  const hldb::ArrayTypespec *const outer = arr2->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(outer, nullptr);
  const hldb::ArrayTypespec *const inner = outer->getElemTypespec()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(inner, nullptr);
  ASSERT_NE(inner->getElemTypespec(), nullptr);
  const hldb::BitTypespec *const bt = inner->getElemTypespec()->getActual<hldb::BitTypespec>();
  ASSERT_NE(bt, nullptr);
  ASSERT_NE(bt->getRanges(), nullptr);
  ASSERT_EQ(bt->getRanges()->size(), 2u);
  EXPECT_EQ(bt->getRanges()->at(0)->getLeftExpr<hldb::Constant>()->getDecompile(), "1");
  EXPECT_EQ(bt->getRanges()->at(0)->getRightExpr<hldb::Constant>()->getDecompile(), "5");
  EXPECT_EQ(bt->getRanges()->at(1)->getLeftExpr<hldb::Constant>()->getDecompile(), "1");
  EXPECT_EQ(bt->getRanges()->at(1)->getRightExpr<hldb::Constant>()->getDecompile(), "6");
}

// --- design-level typespecs / structural completeness ------------------------

TEST_F(MultiDimBasicTest, DesignHasTwoTypespecs) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 2u);
}

TEST_F(MultiDimBasicTest, DesignHasModuleTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const hldb::ModuleTypespec *const mt = any_cast<hldb::ModuleTypespec>(m_design->getTypespecs()->at(0));
  ASSERT_NE(mt, nullptr);
  EXPECT_EQ(mt->getName(), "work@top");
}

TEST_F(MultiDimBasicTest, DesignHasSignedIntTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const hldb::IntTypespec *const it = any_cast<hldb::IntTypespec>(m_design->getTypespecs()->at(1));
  ASSERT_NE(it, nullptr);
  EXPECT_TRUE(it->getSigned());
}

TEST_F(MultiDimBasicTest, NoProcesses) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getProcesses(), nullptr);
}

TEST_F(MultiDimBasicTest, NoContAssigns) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getContAssigns(), nullptr);
}

TEST_F(MultiDimBasicTest, CompilerReportsZeroErrors) {
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
