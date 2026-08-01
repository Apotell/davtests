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

// Tests for basic.sv (tags: 7.4.1 7.4)
//   module top ();
//     bit [7:0] _bit;
//     logic [7:0] _logic;
//     reg [7:0] _reg;
//   endmodule
//
// Checked:
//   - design has module top with exactly 3 variables: "_bit", "_logic", "_reg"
//   - variable "_bit": RefTypespec -> BitTypespec, 1 range [7:0], vector=true
//   - variable "_logic": RefTypespec -> LogicTypespec, 1 range [7:0], vector=true
//   - variable "_reg": RefTypespec -> LogicTypespec (NOT a distinct "RegTypespec"
//     -- the "reg" keyword maps to the same LogicTypespec as "logic"), 1
//     range [7:0], vector=true
//   - module has exactly 3 typespecs (1 BitTypespec + 2 LogicTypespec)
//   - design-level typespecs (2): ModuleTypespec, IntTypespec (signed) -- no
//     StringTypespec since there is no initial block / $display
//   - module has no processes (pure declarations, no initial/always block)
//   - no continuous assignments
//   - compiler emits zero errors
//
// 

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/bit_typespec.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/int_typespec.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/variable.h>
#include <hldb/range.h>
#include <hldb/ref_typespec.h>

namespace hlc {

class PackedBasicTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "basic.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// --- module ----

TEST_F(PackedBasicTest, ModuleExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  EXPECT_NE(top, nullptr);
}

TEST_F(PackedBasicTest, ModuleHasThreeVariables) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_EQ(top->getVariables()->size(), 3u);
}

TEST_F(PackedBasicTest, ModuleHasThreeTypespecs) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTypespecs(), nullptr);
  EXPECT_EQ(top->getTypespecs()->size(), 3u);
}

// --- variable _bit: bit [7:0] ----

TEST_F(PackedBasicTest, VariableBitNameIsBit) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const bit = hldb::findByName<hldb::Variable>("_bit", top->getVariables());
  ASSERT_NE(bit, nullptr);
  EXPECT_EQ(bit->getName(), "_bit");
}

TEST_F(PackedBasicTest, VariableBitIsBitTypespecRange7to0) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const bit = hldb::findByName<hldb::Variable>("_bit", top->getVariables());
  ASSERT_NE(bit, nullptr);
  const hldb::BitTypespec *const bt = bit->getTypespec<hldb::RefTypespec>()->getActual<hldb::BitTypespec>();
  ASSERT_NE(bt, nullptr);
  EXPECT_TRUE(bt->getVector());
  ASSERT_NE(bt->getRanges(), nullptr);
  ASSERT_EQ(bt->getRanges()->size(), 1u);
  EXPECT_EQ(bt->getRanges()->at(0)->getLeftExpr<hldb::Constant>()->getDecompile(), "7");
  EXPECT_EQ(bt->getRanges()->at(0)->getRightExpr<hldb::Constant>()->getDecompile(), "0");
}

// --- variable _logic: logic [7:0] ----

TEST_F(PackedBasicTest, VariableLogicNameIsLogic) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const logic = hldb::findByName<hldb::Variable>("_logic", top->getVariables());
  ASSERT_NE(logic, nullptr);
  EXPECT_EQ(logic->getName(), "_logic");
}

TEST_F(PackedBasicTest, VariableLogicIsLogicTypespecRange7to0) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const logic = hldb::findByName<hldb::Variable>("_logic", top->getVariables());
  ASSERT_NE(logic, nullptr);
  const hldb::LogicTypespec *const lt = logic->getTypespec<hldb::RefTypespec>()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(lt, nullptr);
  EXPECT_TRUE(lt->getVector());
  ASSERT_NE(lt->getRanges(), nullptr);
  ASSERT_EQ(lt->getRanges()->size(), 1u);
  EXPECT_EQ(lt->getRanges()->at(0)->getLeftExpr<hldb::Constant>()->getDecompile(), "7");
  EXPECT_EQ(lt->getRanges()->at(0)->getRightExpr<hldb::Constant>()->getDecompile(), "0");
}

// --- variable _reg: reg [7:0] ----

TEST_F(PackedBasicTest, VariableRegNameIsReg) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const reg = hldb::findByName<hldb::Variable>("_reg", top->getVariables());
  ASSERT_NE(reg, nullptr);
  EXPECT_EQ(reg->getName(), "_reg");
}

TEST_F(PackedBasicTest, VariableRegIsLogicTypespecNotADistinctRegType) {
  // COMPILER BEHAVIOR: "reg" is not a distinct typespec kind; it maps to the
  // same LogicTypespec as "logic".
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const reg = hldb::findByName<hldb::Variable>("_reg", top->getVariables());
  ASSERT_NE(reg, nullptr);
  const hldb::LogicTypespec *const lt = reg->getTypespec<hldb::RefTypespec>()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(lt, nullptr);
  EXPECT_TRUE(lt->getVector());
  ASSERT_NE(lt->getRanges(), nullptr);
  ASSERT_EQ(lt->getRanges()->size(), 1u);
  EXPECT_EQ(lt->getRanges()->at(0)->getLeftExpr<hldb::Constant>()->getDecompile(), "7");
  EXPECT_EQ(lt->getRanges()->at(0)->getRightExpr<hldb::Constant>()->getDecompile(), "0");
}

// --- design-level typespecs / structural completeness ----

TEST_F(PackedBasicTest, DesignHasTwoTypespecs) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 2u);
}

TEST_F(PackedBasicTest, DesignHasModuleTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const hldb::ModuleTypespec *const mt = any_cast<hldb::ModuleTypespec>(m_design->getTypespecs()->at(0));
  ASSERT_NE(mt, nullptr);
  EXPECT_EQ(mt->getName(), "top");
}

TEST_F(PackedBasicTest, DesignHasSignedIntTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const hldb::IntTypespec *const it = any_cast<hldb::IntTypespec>(m_design->getTypespecs()->at(1));
  ASSERT_NE(it, nullptr);
  EXPECT_TRUE(it->getSigned());
}

TEST_F(PackedBasicTest, NoProcesses) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getProcesses(), nullptr);
}

TEST_F(PackedBasicTest, NoContAssigns) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getContAssigns(), nullptr);
}

TEST_F(PackedBasicTest, CompilerReportsZeroErrors) {
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
