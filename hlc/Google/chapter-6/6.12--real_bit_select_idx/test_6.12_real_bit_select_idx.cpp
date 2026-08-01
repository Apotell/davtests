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
// used as a bit-select index into a packed wire:
//   module top();
//     real a = 0.5;
//     wire [3:0] b;
//     wire c;
//     assign c = b[a];
//   endmodule
//
// Checked:
//   - design has module top
//   - module has exactly 2 nets: 'b' (wire[3:0]), 'c' (wire); 'a' (real) is a Variable
//   - 'a' typespec -> RealTypespec; initial value vpiRealConst "0.5"
//   - 'b' and 'c' net types are vpiWire; no initial values
//   - 1 ContAssign: LHS RefObj "c" resolves to net 'c', RHS = BitSelect "b[a]"
//   - BitSelect prefix RefObj "b" resolves to net 'b'
//   - BitSelect index RefObj "a" resolves to the real Variable 'a'
//   - top has no processes
//
// Also checked:
//   - Per IEEE 1800-2023 Sec 6.12: "Real numbers and real variables are ...
//     prohibited in the following cases: ... Real index expressions of
//     bit-selects or part-selects of vectors." 'b[a]' with real index 'a' is
//     illegal SystemVerilog. HLC currently does not report a compile-time
//     error for this (known gap) -- see the GTEST_SKIP()'d test below.

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

class RealBitSelectIdx : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.12--real_bit_select_idx.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(RealBitSelectIdx, ModuleExists) {
  ASSERT_NE(hldb::findByName<hldb::Module>("top", m_design->getAllModules()), nullptr);
}

// ----
// Net declarations -- real 'a' (Variable), wire[3:0] 'b' (Net), wire 'c' (Net)
// ----
TEST_F(RealBitSelectIdx, TwoNetsExist) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 2u) << "expected nets 'b' (wire[3:0]) and 'c' (wire)";
}

TEST_F(RealBitSelectIdx, OneVariableExist) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_EQ(top->getVariables()->size(), 1u) << "expected variables 'a' (real)";
}

TEST_F(RealBitSelectIdx, AVariableTypespecIsReal) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr) << "variable 'a' not found";

  const hldb::RefTypespec *const rts = a->getTypespec();
  ASSERT_NE(rts, nullptr) << "variable 'a' has no typespec";
  EXPECT_NE(rts->getActual<hldb::RealTypespec>(), nullptr) << "variable 'a' typespec should resolve to RealTypespec";
}

TEST_F(RealBitSelectIdx, AVariableInitialValueIsHalf) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  const hldb::Constant *const init = a->getValue<hldb::Constant>();
  ASSERT_NE(init, nullptr);
  EXPECT_EQ(init->getConstType(), vpiRealConst);
  EXPECT_EQ(init->getDecompile(), "0.5");
}

TEST_F(RealBitSelectIdx, BNetIsWire) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const b = hldb::findByName<hldb::Net>("b", top->getNets());
  ASSERT_NE(b, nullptr) << "net 'b' not found";
  EXPECT_EQ(b->getNetType(), vpiWire);
}

TEST_F(RealBitSelectIdx, CNetIsWire) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const c = hldb::findByName<hldb::Net>("c", top->getNets());
  ASSERT_NE(c, nullptr) << "net 'c' not found";
  EXPECT_EQ(c->getNetType(), vpiWire);
}

// ----
// Continuous assignment -- assign c = b[a]
// ----
TEST_F(RealBitSelectIdx, ContAssignExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  EXPECT_EQ(top->getContAssigns()->size(), 1u);
}

TEST_F(RealBitSelectIdx, ContAssignLhsIsC) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);

  const hldb::RefObj *const lhs = top->getContAssigns()->at(0)->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr) << "ContAssign LHS is not a RefObj";
  EXPECT_EQ(lhs->getName(), "c");
}

TEST_F(RealBitSelectIdx, ContAssignRhsIsBitSelect) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);

  const hldb::BitSelect *const rhs = top->getContAssigns()->at(0)->getRhs<hldb::BitSelect>();
  ASSERT_NE(rhs, nullptr) << "ContAssign RHS is not a BitSelect";
  EXPECT_EQ(rhs->getName(), "b[a]");
}

// ----
// BitSelect internals -- prefix is logic 'b', index is real RefObj 'a'
// ----
TEST_F(RealBitSelectIdx, BitSelectPrefixIsRefObjB) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);

  const hldb::BitSelect *const rhs = top->getContAssigns()->at(0)->getRhs<hldb::BitSelect>();
  ASSERT_NE(rhs, nullptr);

  const hldb::RefObj *const prefix = rhs->getPrefix<hldb::RefObj>();
  ASSERT_NE(prefix, nullptr) << "BitSelect prefix is not a RefObj";
  EXPECT_EQ(prefix->getName(), "b");
}

TEST_F(RealBitSelectIdx, BitSelectPrefixResolvesToNetB) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);

  const hldb::BitSelect *const rhs = top->getContAssigns()->at(0)->getRhs<hldb::BitSelect>();
  ASSERT_NE(rhs, nullptr);
  const hldb::RefObj *const prefix = rhs->getPrefix<hldb::RefObj>();
  ASSERT_NE(prefix, nullptr);

  const hldb::Net *const net = prefix->getActual<hldb::Net>();
  ASSERT_NE(net, nullptr) << "BitSelect prefix does not resolve to a Net";
  EXPECT_EQ(net->getName(), "b");
}

TEST_F(RealBitSelectIdx, BitSelectIndexIsRefObjA) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);

  const hldb::BitSelect *const rhs = top->getContAssigns()->at(0)->getRhs<hldb::BitSelect>();
  ASSERT_NE(rhs, nullptr);

  // Index is a real variable -- illegal in SV but HLDB records it as RefObj
  const hldb::RefObj *const idx = rhs->getIndex<hldb::RefObj>();
  ASSERT_NE(idx, nullptr) << "BitSelect index is not a RefObj";
  EXPECT_EQ(idx->getName(), "a");
}

TEST_F(RealBitSelectIdx, BitSelectIndexResolvesToRealVariable) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);

  const hldb::BitSelect *const rhs = top->getContAssigns()->at(0)->getRhs<hldb::BitSelect>();
  ASSERT_NE(rhs, nullptr);
  const hldb::RefObj *const idx = rhs->getIndex<hldb::RefObj>();
  ASSERT_NE(idx, nullptr);

  const hldb::Variable *const var = idx->getActual<hldb::Variable>();
  ASSERT_NE(var, nullptr) << "BitSelect index RefObj does not resolve to a Variable";
  EXPECT_EQ(var->getName(), "a");

  const hldb::RefTypespec *const rts = var->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::RealTypespec>(), nullptr)
      << "index variable 'a' must be real-typed -- it is the illegal real index";
}

TEST_F(RealBitSelectIdx, ContAssignLhsResolvesToNetC) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  const hldb::RefObj *const lhs = top->getContAssigns()->at(0)->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  const hldb::Net *const net = lhs->getActual<hldb::Net>();
  ASSERT_NE(net, nullptr) << "ContAssign LHS RefObj 'c' should resolve to the formally declared net 'c'";
  EXPECT_EQ(net->getName(), "c");
}

TEST_F(RealBitSelectIdx, BNetHasNoInitialValue) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const b = hldb::findByName<hldb::Net>("b", top->getNets());
  ASSERT_NE(b, nullptr);
  EXPECT_EQ(b->getValue<hldb::Any>(), nullptr) << "wire 'b' is declared without an initializer";
}

TEST_F(RealBitSelectIdx, CNetHasNoInitialValue) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const c = hldb::findByName<hldb::Net>("c", top->getNets());
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getValue<hldb::Any>(), nullptr) << "wire 'c' is declared without an initializer";
}

TEST_F(RealBitSelectIdx, NoProcesses) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getProcesses() == nullptr || top->getProcesses()->empty());
}

// ----
// Compiler diagnostics -- IEEE 1800-2023 Sec 6.12 prohibits real index
// expressions of bit-selects or part-selects of vectors. HLC does not
// currently flag this; see GTEST_SKIP() below.
// ----
TEST_F(RealBitSelectIdx, Compiler_ReportsErrorForIllegalRealIndex) {
  GTEST_SKIP() << "known gap: real-typed bit-select index ('b[a]') is not rejected by HLC; "
                  "IEEE 1800-2023 Sec 6.12 prohibits real index expressions of bit-selects/part-selects";
  const hlc::ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_GE(stats.nbError, 1) << "'b[a]' with real index 'a' must be flagged illegal per Sec 6.12";
}

TEST_F(RealBitSelectIdx, ANotInNets) {
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
