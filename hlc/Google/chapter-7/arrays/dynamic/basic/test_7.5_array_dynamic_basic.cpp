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

// Tests for basic.sv (tags: 7.5)
//   module top ();
//     bit [7:0] arr[];
//   endmodule
//
// Checked:
//   - design has module top
//   - module has exactly 1 variable: "arr" (RefTypespec->ArrayTypespec)
//   - ArrayTypespec: vpiArrayType=dynamic(2) -- key distinction from static(1) and associative(3)
//   - ArrayTypespec ElemTypespec: RefTypespec->BitTypespec
//   - BitTypespec: vpiVector=true, getSigned()==false, 1 Range [7:0] (left=7, right=0)
//   - Range constants are vpiUIntConst
//   - variable has no initial value
//   - design has 2 typespecs: ModuleTypespec "top" + IntTypespec (signed)
//   - top has no processes, no continuous assignments
//
// Not checked:
//   - vpiVariableType (not set for bit-type variables -- getVariableType() returns 0)
//   - Variable::getVector() (false -- vpiVector only set on BitTypespec, not on Variable node)

#include <hlc/Common/Session.h>
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

class DynArrBasic : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "basic.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// --- module ----

TEST_F(DynArrBasic, ModuleExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  EXPECT_NE(top, nullptr);
}

// --- variable arr ----

TEST_F(DynArrBasic, ModuleHasOneVariable) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_EQ(top->getVariables()->size(), 1u);
}

TEST_F(DynArrBasic, VariableNameIsArr) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_EQ(top->getVariables()->at(0)->getName(), "arr");
}

TEST_F(DynArrBasic, VariableHasNoInitialValue) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_EQ(top->getVariables()->at(0)->getValue(), nullptr);
}

// --- ArrayTypespec ----

TEST_F(DynArrBasic, VariableHasArrayTypespec) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const variable = top->getVariables()->at(0);
  ASSERT_NE(variable, nullptr);
  const hldb::RefTypespec *const rt = variable->getTypespec<hldb::RefTypespec>();
  ASSERT_NE(rt, nullptr);
  EXPECT_NE(rt->getActual<hldb::ArrayTypespec>(), nullptr);
}

TEST_F(DynArrBasic, ArrayTypespecIsDynamic) {
  // vpiArrayType: dynamic (2) -- distinguishes from static(1) and associative(3)
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::ArrayTypespec *const at =
      top->getVariables()->at(0)->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  EXPECT_EQ(at->getArrayType(), 2);  // dynamic = 2
}

TEST_F(DynArrBasic, ArrayTypespecHasElemTypespec) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::ArrayTypespec *const at =
      top->getVariables()->at(0)->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getElemTypespec(), nullptr);
  EXPECT_NE(at->getElemTypespec()->getActual<hldb::BitTypespec>(), nullptr);
}

TEST_F(DynArrBasic, ArrayTypespecHasNoIndexTypespec) {
  // Dynamic arrays have no index typespec (only associative arrays do)
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::ArrayTypespec *const at =
      top->getVariables()->at(0)->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  EXPECT_EQ(at->getIndexTypespec(), nullptr);
}

// --- BitTypespec ----

TEST_F(DynArrBasic, BitTypespecIsVector) {
  // bit [7:0] is multi-bit -- vpiVector=true on BitTypespec
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::BitTypespec *const bt = top->getVariables()
                                          ->at(0)
                                          ->getTypespec<hldb::RefTypespec>()
                                          ->getActual<hldb::ArrayTypespec>()
                                          ->getElemTypespec()
                                          ->getActual<hldb::BitTypespec>();
  ASSERT_NE(bt, nullptr);
  EXPECT_TRUE(bt->getVector());
}

TEST_F(DynArrBasic, BitTypespecIsNotSigned) {
  // `bit` is unsigned by default
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::BitTypespec *const bt = top->getVariables()
                                          ->at(0)
                                          ->getTypespec<hldb::RefTypespec>()
                                          ->getActual<hldb::ArrayTypespec>()
                                          ->getElemTypespec()
                                          ->getActual<hldb::BitTypespec>();
  ASSERT_NE(bt, nullptr);
  EXPECT_FALSE(bt->getSigned());
}

TEST_F(DynArrBasic, BitTypespecIsNotScalar) {
  // [7:0] makes this a vector, not a scalar 1-bit type
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::BitTypespec *const bt = top->getVariables()
                                          ->at(0)
                                          ->getTypespec<hldb::RefTypespec>()
                                          ->getActual<hldb::ArrayTypespec>()
                                          ->getElemTypespec()
                                          ->getActual<hldb::BitTypespec>();
  ASSERT_NE(bt, nullptr);
  EXPECT_FALSE(bt->getScalar());
}

TEST_F(DynArrBasic, BitTypespecHasNoIndexTypespec) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::BitTypespec *const bt = top->getVariables()
                                          ->at(0)
                                          ->getTypespec<hldb::RefTypespec>()
                                          ->getActual<hldb::ArrayTypespec>()
                                          ->getElemTypespec()
                                          ->getActual<hldb::BitTypespec>();
  ASSERT_NE(bt, nullptr);
  EXPECT_EQ(bt->getIndexTypespec(), nullptr);
}

// --- Range [7:0] ----

TEST_F(DynArrBasic, BitTypespecHasOneRange) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::BitTypespec *const bt = top->getVariables()
                                          ->at(0)
                                          ->getTypespec<hldb::RefTypespec>()
                                          ->getActual<hldb::ArrayTypespec>()
                                          ->getElemTypespec()
                                          ->getActual<hldb::BitTypespec>();
  ASSERT_NE(bt, nullptr);
  ASSERT_NE(bt->getRanges(), nullptr);
  EXPECT_EQ(bt->getRanges()->size(), 1u);
}

TEST_F(DynArrBasic, RangeLeftIs7) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::BitTypespec *const bt = top->getVariables()
                                          ->at(0)
                                          ->getTypespec<hldb::RefTypespec>()
                                          ->getActual<hldb::ArrayTypespec>()
                                          ->getElemTypespec()
                                          ->getActual<hldb::BitTypespec>();
  ASSERT_NE(bt, nullptr);
  ASSERT_NE(bt->getRanges(), nullptr);
  const hldb::Constant *const left = bt->getRanges()->at(0)->getLeftExpr<hldb::Constant>();
  ASSERT_NE(left, nullptr);
  EXPECT_EQ(left->getDecompile(), "7");
}

TEST_F(DynArrBasic, RangeRightIs0) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::BitTypespec *const bt = top->getVariables()
                                          ->at(0)
                                          ->getTypespec<hldb::RefTypespec>()
                                          ->getActual<hldb::ArrayTypespec>()
                                          ->getElemTypespec()
                                          ->getActual<hldb::BitTypespec>();
  ASSERT_NE(bt, nullptr);
  ASSERT_NE(bt->getRanges(), nullptr);
  const hldb::Constant *const right = bt->getRanges()->at(0)->getRightExpr<hldb::Constant>();
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(right->getDecompile(), "0");
}

TEST_F(DynArrBasic, RangeLeftConstTypeIsUInt) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::BitTypespec *const bt = top->getVariables()
                                          ->at(0)
                                          ->getTypespec<hldb::RefTypespec>()
                                          ->getActual<hldb::ArrayTypespec>()
                                          ->getElemTypespec()
                                          ->getActual<hldb::BitTypespec>();
  ASSERT_NE(bt, nullptr);
  ASSERT_NE(bt->getRanges(), nullptr);
  const hldb::Constant *const left = bt->getRanges()->at(0)->getLeftExpr<hldb::Constant>();
  ASSERT_NE(left, nullptr);
  EXPECT_EQ(left->getConstType(), vpiUIntConst);
}

TEST_F(DynArrBasic, RangeRightConstTypeIsUInt) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::BitTypespec *const bt = top->getVariables()
                                          ->at(0)
                                          ->getTypespec<hldb::RefTypespec>()
                                          ->getActual<hldb::ArrayTypespec>()
                                          ->getElemTypespec()
                                          ->getActual<hldb::BitTypespec>();
  ASSERT_NE(bt, nullptr);
  ASSERT_NE(bt->getRanges(), nullptr);
  const hldb::Constant *const right = bt->getRanges()->at(0)->getRightExpr<hldb::Constant>();
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(right->getConstType(), vpiUIntConst);
}

// --- design-level typespecs ----

TEST_F(DynArrBasic, DesignHasTwoTypespecs) {
  // log: vpiTypespec (2 items): ModuleTypespec "top" + IntTypespec
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 2u);
}

TEST_F(DynArrBasic, DesignHasModuleTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const hldb::ModuleTypespec *const mt = any_cast<hldb::ModuleTypespec>(m_design->getTypespecs()->at(0));
  ASSERT_NE(mt, nullptr);
  EXPECT_EQ(mt->getName(), "top");
}

TEST_F(DynArrBasic, DesignHasIntTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const hldb::IntTypespec *const it = any_cast<hldb::IntTypespec>(m_design->getTypespecs()->at(1));
  EXPECT_NE(it, nullptr);
}

// --- structural completeness ----

TEST_F(DynArrBasic, NoProcesses) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getProcesses(), nullptr);
}

TEST_F(DynArrBasic, NoContAssigns) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getContAssigns(), nullptr);
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
