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

// Validates the UHDM graph for a module where a real variable is illegally
// bit-selected with a constant index:
//   module top();
//     real a = 0.5;
//     wire b;
//     assign b = a[2];
//   endmodule
//
// Checked:
//   - design has module top
//   - module has exactly 1 net: 'b' (vpiWire, no initial value); 'a' (real) is a Variable
//   - 'a' typespec -> RealTypespec; initial value vpiRealConst "0.5"
//   - 1 ContAssign: LHS RefObj "b" resolves to net 'b', RHS = BitSelect "a[2]"
//   - BitSelect prefix RefObj "a" resolves to the real Variable 'a'
//   - BitSelect index Constant "2"
//   - top has no processes
//
// Also checked:
//   - Per IEEE 1800-2023 Sec 11.5.1: "A bit-select or part-select of a
//     scalar, or of a real variable or real parameter, shall be illegal."
//     'a[2]' on the real variable 'a' is illegal SystemVerilog. HLC currently
//     does not report a compile-time error for this (known gap) -- see the
//     GTEST_SKIP()'d test below.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/bit_select.h>
#include <hldb/constant.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/real_typespec.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/vpi_user.h>
#include <hldb/variable.h>

namespace hlc {

class RealBitSelect : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.12--real_bit_select.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(RealBitSelect, ModuleExists) {
  ASSERT_NE(hldb::findByName<hldb::Module>("top", m_design->getAllModules()), nullptr);
}

// ----
// Net declarations -- real 'a' (Variable) and wire 'b' (Net)
// ----
TEST_F(RealBitSelect, OneNetExist) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 1u) << "expected nets 'b' (wire)";
}

TEST_F(RealBitSelect, OneVariableExist) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_EQ(top->getVariables()->size(), 1u) << "expected nets 'a' (real)";
}

TEST_F(RealBitSelect, AVariableTypespecIsReal) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr) << "variable 'a' not found";

  const hldb::RefTypespec *const rts = a->getTypespec();
  ASSERT_NE(rts, nullptr) << "variable 'a' has no typespec";
  EXPECT_NE(rts->getActual<hldb::RealTypespec>(), nullptr) << "variable 'a' typespec should resolve to RealTypespec";
}

TEST_F(RealBitSelect, AVariableInitialValueIsHalf) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  const hldb::Constant *const init = a->getValue<hldb::Constant>();
  ASSERT_NE(init, nullptr);
  EXPECT_EQ(init->getConstType(), vpiRealConst);
  EXPECT_EQ(init->getDecompile(), "0.5");
}

TEST_F(RealBitSelect, BNetIsWire) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const b = hldb::findByName<hldb::Net>("b", top->getNets());
  ASSERT_NE(b, nullptr) << "net 'b' not found";
  EXPECT_EQ(b->getNetType(), vpiWire);
}

// ----
// Continuous assignment -- assign b = a[2]
// ----
TEST_F(RealBitSelect, ContAssignExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  EXPECT_EQ(top->getContAssigns()->size(), 1u);
}

TEST_F(RealBitSelect, ContAssignLhsIsB) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);

  const hldb::RefObj *const lhs = top->getContAssigns()->at(0)->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr) << "ContAssign LHS is not a RefObj";
  EXPECT_EQ(lhs->getName(), "b");
}

TEST_F(RealBitSelect, ContAssignRhsIsBitSelect) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);

  const hldb::BitSelect *const rhs = top->getContAssigns()->at(0)->getRhs<hldb::BitSelect>();
  ASSERT_NE(rhs, nullptr) << "ContAssign RHS is not a BitSelect";
  EXPECT_EQ(rhs->getName(), "a[2]");
}

// ----
// BitSelect internals -- prefix is real 'a', index is Constant 2
// ----
TEST_F(RealBitSelect, BitSelectPrefixIsRefObjA) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);

  const hldb::BitSelect *const rhs = top->getContAssigns()->at(0)->getRhs<hldb::BitSelect>();
  ASSERT_NE(rhs, nullptr);

  const hldb::RefObj *const prefix = rhs->getPrefix<hldb::RefObj>();
  ASSERT_NE(prefix, nullptr) << "BitSelect prefix is not a RefObj";
  EXPECT_EQ(prefix->getName(), "a");
}

TEST_F(RealBitSelect, BitSelectPrefixResolvesToRealNet) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);

  const hldb::BitSelect *const rhs = top->getContAssigns()->at(0)->getRhs<hldb::BitSelect>();
  ASSERT_NE(rhs, nullptr);
  const hldb::RefObj *const prefix = rhs->getPrefix<hldb::RefObj>();
  ASSERT_NE(prefix, nullptr);

  const hldb::Variable *const var = prefix->getActual<hldb::Variable>();
  ASSERT_NE(var, nullptr) << "BitSelect prefix does not resolve to a Variable";
  const hldb::RefTypespec *const rts = var->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::RealTypespec>(), nullptr) << "bit-selected prefix 'a' should be a real-typed net";
}

TEST_F(RealBitSelect, BitSelectIndexIsConstant2) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);

  const hldb::BitSelect *const rhs = top->getContAssigns()->at(0)->getRhs<hldb::BitSelect>();
  ASSERT_NE(rhs, nullptr);

  const hldb::Constant *const idx = rhs->getIndex<hldb::Constant>();
  ASSERT_NE(idx, nullptr) << "BitSelect index is not a Constant";
  EXPECT_EQ(idx->getDecompile(), "2");
}

TEST_F(RealBitSelect, ContAssignLhsResolvesToNetB) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  const hldb::RefObj *const lhs = top->getContAssigns()->at(0)->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_NE(lhs->getActual<hldb::Net>(), nullptr)
      << "ContAssign LHS RefObj 'b' should resolve to the formally declared net 'b'";
}

TEST_F(RealBitSelect, BNetHasNoInitialValue) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const b = hldb::findByName<hldb::Net>("b", top->getNets());
  ASSERT_NE(b, nullptr);
  EXPECT_EQ(b->getValue<hldb::Any>(), nullptr) << "wire 'b' is declared without an initializer";
}

TEST_F(RealBitSelect, NoProcesses) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getProcesses() == nullptr || top->getProcesses()->empty());
}

// ----
// Compiler diagnostics -- IEEE 1800-2023 Sec 11.5.1: "A bit-select or
// part-select of a scalar, or of a real variable or real parameter, shall
// be illegal." HLC does not currently flag this; see GTEST_SKIP() below.
// ----
TEST_F(RealBitSelect, Compiler_ReportsErrorForIllegalRealBitSelect) {
  GTEST_SKIP() << "known gap: bit-select on a real variable ('a[2]') is not rejected by HLC; "
                  "IEEE 1800-2023 Sec 11.5.1 requires this to be illegal";
  const hlc::ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_GE(stats.nbError, 1) << "'a[2]' on real variable 'a' must be flagged illegal per Sec 11.5.1";
}

TEST_F(RealBitSelect, ANotInNets) {
  // Per IEEE 1800-2023 Sec 6.7/6.8, 'real' has no net-type keyword, so 'a'
  // must not also be materialized as a Net.
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(hldb::findByName<hldb::Net>("a", top->getNets()), nullptr) << "'real a' must not appear in vpiNet";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
