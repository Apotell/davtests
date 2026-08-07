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

// Tests for basic.sv (tags: 7.4.2 7.4)
//   module top ();
//     bit _bit [7:0];
//     logic _logic [7:0];
//     reg _reg [7:0];
//   endmodule
//
// Checked:
//   - design has module top with exactly 3 variables: "_bit", "_logic",
//     "_reg" (IEEE 1800-2023 6.7/6.8: a declaration with a data type and no
//     net-type keyword is a variable_declaration, never a net_declaration)
//   - none of the three appear in getNets() (no cross-collection duplicate)
//   - all 3 variables: RefTypespec -> ArrayTypespec static(1), range [7:0]
//     (unpacked dimension)
//   - variable "_bit": ArrayTypespec elem -> BitTypespec
//   - variable "_logic": ArrayTypespec elem -> LogicTypespec
//   - variable "_reg": ArrayTypespec elem -> LogicTypespec -- "reg" maps to
//     the SAME LogicTypespec instance as "_logic"'s element type (reg is not
//     a distinct typespec kind), matching the analogous finding in
//     chapter-7/arrays/packed/basic
//   - module has exactly 5 typespecs: 1 BitTypespec + 1 ArrayTypespec (for
//     "_bit") + 1 LogicTypespec (shared by "_logic" and "_reg") + 2
//     ArrayTypespec (for "_logic" and "_reg", each its own distinct
//     instance despite identical [7:0] range)
//   - design-level typespecs (2): ModuleTypespec, IntTypespec (signed) --
//     no StringTypespec since there is no initial block / $display
//   - module has no processes (pure declaration, no initial/always block)
//   - no continuous assignments
//   - compiler emits zero errors
//
// Not checked:
//   - none -- basic.sv is declaration-only with no runtime behavior to
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
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/net.h>
#include <hldb/range.h>
#include <hldb/ref_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class UnpackedBasicTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "basic.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// --- module / nets ----

TEST_F(UnpackedBasicTest, ModuleExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  EXPECT_NE(top, nullptr);
}

TEST_F(UnpackedBasicTest, ModuleHasThreeVariables) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_EQ(top->getVariables()->size(), 3u)
      << "6.7/6.8: 'bit'/'logic'/'reg' declared with no net-type keyword are variables";
}

TEST_F(UnpackedBasicTest, ModuleHasNoNets) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getNets(), nullptr) << "no net-type keyword is present in basic.sv";
}

TEST_F(UnpackedBasicTest, ModuleHasFiveTypespecs) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTypespecs(), nullptr);
  EXPECT_EQ(top->getTypespecs()->size(), 5u);
}

TEST_F(UnpackedBasicTest, BitNetIsArrayOfBitTypespecRangeSevenToZero) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const bit = hldb::findByName<hldb::Variable>("_bit", top->getVariables());
  ASSERT_NE(bit, nullptr);
  const hldb::ArrayTypespec *const at = bit->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  EXPECT_EQ(at->getArrayType(), 1);  // static = 1
  ASSERT_NE(at->getRange(), nullptr);
  EXPECT_EQ(at->getRange()->getLeftExpr<hldb::Constant>()->getDecompile(), "7");
  EXPECT_EQ(at->getRange()->getRightExpr<hldb::Constant>()->getDecompile(), "0");
  EXPECT_NE(at->getElemTypespec()->getActual<hldb::BitTypespec>(), nullptr);
}

TEST_F(UnpackedBasicTest, LogicNetIsArrayOfLogicTypespec) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const logic = hldb::findByName<hldb::Variable>("_logic", top->getVariables());
  ASSERT_NE(logic, nullptr);
  const hldb::ArrayTypespec *const at = logic->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  EXPECT_NE(at->getElemTypespec()->getActual<hldb::LogicTypespec>(), nullptr);
}

TEST_F(UnpackedBasicTest, RegNetMapsToSameLogicTypespecAsLogicNet) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const logic = hldb::findByName<hldb::Variable>("_logic", top->getVariables());
  const hldb::Variable *const reg = hldb::findByName<hldb::Variable>("_reg", top->getVariables());
  ASSERT_NE(logic, nullptr);
  ASSERT_NE(reg, nullptr);
  const hldb::ArrayTypespec *const atLogic = logic->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  const hldb::ArrayTypespec *const atReg = reg->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(atLogic, nullptr);
  ASSERT_NE(atReg, nullptr);
  EXPECT_NE(atLogic, atReg) << "each net should still get its own distinct ArrayTypespec instance";
  const hldb::LogicTypespec *const logicElem = atLogic->getElemTypespec()->getActual<hldb::LogicTypespec>();
  const hldb::LogicTypespec *const regElem = atReg->getElemTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(logicElem, nullptr);
  ASSERT_NE(regElem, nullptr);
  EXPECT_EQ(logicElem, regElem) << "'reg' should map to the same LogicTypespec kind as 'logic', not a "
                                   "distinct typespec";
}

// --- design-level typespecs / compiler diagnostics ----

TEST_F(UnpackedBasicTest, DesignHasTwoTypespecs) {
  // No StringTypespec: basic.sv has no initial block / $display.
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 2u);
}

TEST_F(UnpackedBasicTest, DesignHasModuleTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const hldb::ModuleTypespec *const mt = any_cast<hldb::ModuleTypespec>(m_design->getTypespecs()->at(0));
  ASSERT_NE(mt, nullptr);
  EXPECT_EQ(mt->getName(), "top");
}

TEST_F(UnpackedBasicTest, DesignHasSignedIntTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const hldb::IntTypespec *const it = any_cast<hldb::IntTypespec>(m_design->getTypespecs()->at(1));
  ASSERT_NE(it, nullptr);
  EXPECT_TRUE(it->getSigned());
}

TEST_F(UnpackedBasicTest, ModuleHasNoProcesses) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getProcesses(), nullptr);
}

TEST_F(UnpackedBasicTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
  EXPECT_EQ(stats.nbWarning, 0);
}

TEST_F(UnpackedBasicTest, NoContAssigns) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getContAssigns(), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
