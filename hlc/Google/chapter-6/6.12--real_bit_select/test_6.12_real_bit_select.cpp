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
//   - design has module work@top
//   - module has exactly 2 nets: 'a' (real) and 'b' (vpiWire, no initial value)
//   - 'a' typespec → RealTypespec; initial value vpiRealConst "0.5"
//   - 1 ContAssign: LHS RefObj "b" resolves to net 'b', RHS = BitSelect "a[2]"
//   - BitSelect prefix RefObj "a" resolves to the real Net 'a'
//   - BitSelect index Constant "2"
//   - work@top has no processes
//
// Not checked:
//   - HLC doesn't flag the illegal bit-select on real

#include <hlc/Common/Session.h>
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

namespace hlc {

class RealBitSelect : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.12--real_bit_select.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(RealBitSelect, ModuleExists) {
  ASSERT_NE(hldb::findByName<hldb::Module>("work@top", m_design->getAllModules()), nullptr);
}

// ---------------------------------------------------------------------------
// Net declarations — real 'a' and wire 'b'
// ---------------------------------------------------------------------------
TEST_F(RealBitSelect, TwoNetsExist) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 2u) << "expected nets 'a' (real) and 'b' (wire)";
}

TEST_F(RealBitSelect, ANetTypespecIsReal) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const a = hldb::findByName<hldb::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr) << "net 'a' not found";

  const hldb::RefTypespec *const rts = a->getTypespec();
  ASSERT_NE(rts, nullptr) << "net 'a' has no typespec";
  EXPECT_NE(rts->getActual<hldb::RealTypespec>(), nullptr) << "net 'a' typespec should resolve to RealTypespec";
}

TEST_F(RealBitSelect, ANetInitialValueIsHalf) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const a = hldb::findByName<hldb::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr);
  const hldb::Constant *const init = a->getValue<hldb::Constant>();
  ASSERT_NE(init, nullptr);
  EXPECT_EQ(init->getConstType(), vpiRealConst);
  EXPECT_EQ(init->getDecompile(), "0.5");
}

TEST_F(RealBitSelect, BNetIsWire) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const b = hldb::findByName<hldb::Net>("b", top->getNets());
  ASSERT_NE(b, nullptr) << "net 'b' not found";
  EXPECT_EQ(b->getNetType(), vpiWire);
}

// ---------------------------------------------------------------------------
// Continuous assignment — assign b = a[2]
// ---------------------------------------------------------------------------
TEST_F(RealBitSelect, ContAssignExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  EXPECT_EQ(top->getContAssigns()->size(), 1u);
}

TEST_F(RealBitSelect, ContAssignLhsIsB) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);

  const hldb::RefObj *const lhs = top->getContAssigns()->at(0)->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr) << "ContAssign LHS is not a RefObj";
  EXPECT_EQ(lhs->getName(), "b");
}

TEST_F(RealBitSelect, ContAssignRhsIsBitSelect) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);

  const hldb::BitSelect *const rhs = top->getContAssigns()->at(0)->getRhs<hldb::BitSelect>();
  ASSERT_NE(rhs, nullptr) << "ContAssign RHS is not a BitSelect";
  EXPECT_EQ(rhs->getName(), "a[2]");
}

// ---------------------------------------------------------------------------
// BitSelect internals — prefix is real 'a', index is Constant 2
// ---------------------------------------------------------------------------
TEST_F(RealBitSelect, BitSelectPrefixIsRefObjA) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);

  const hldb::BitSelect *const rhs = top->getContAssigns()->at(0)->getRhs<hldb::BitSelect>();
  ASSERT_NE(rhs, nullptr);

  const hldb::RefObj *const prefix = rhs->getPrefix<hldb::RefObj>();
  ASSERT_NE(prefix, nullptr) << "BitSelect prefix is not a RefObj";
  EXPECT_EQ(prefix->getName(), "a");
}

TEST_F(RealBitSelect, BitSelectPrefixResolvesToRealNet) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);

  const hldb::BitSelect *const rhs = top->getContAssigns()->at(0)->getRhs<hldb::BitSelect>();
  ASSERT_NE(rhs, nullptr);
  const hldb::RefObj *const prefix = rhs->getPrefix<hldb::RefObj>();
  ASSERT_NE(prefix, nullptr);

  const hldb::Net *const net = prefix->getActual<hldb::Net>();
  ASSERT_NE(net, nullptr) << "BitSelect prefix does not resolve to a Net";
  const hldb::RefTypespec *const rts = net->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::RealTypespec>(), nullptr) << "bit-selected prefix 'a' should be a real-typed net";
}

TEST_F(RealBitSelect, BitSelectIndexIsConstant2) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);

  const hldb::BitSelect *const rhs = top->getContAssigns()->at(0)->getRhs<hldb::BitSelect>();
  ASSERT_NE(rhs, nullptr);

  const hldb::Constant *const idx = rhs->getIndex<hldb::Constant>();
  ASSERT_NE(idx, nullptr) << "BitSelect index is not a Constant";
  EXPECT_EQ(idx->getDecompile(), "2");
}

TEST_F(RealBitSelect, ContAssignLhsResolvesToNetB) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  const hldb::RefObj *const lhs = top->getContAssigns()->at(0)->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_NE(lhs->getActual<hldb::Net>(), nullptr)
      << "ContAssign LHS RefObj 'b' should resolve to the formally declared net 'b'";
}

TEST_F(RealBitSelect, BNetHasNoInitialValue) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const b = hldb::findByName<hldb::Net>("b", top->getNets());
  ASSERT_NE(b, nullptr);
  EXPECT_EQ(b->getValue<hldb::Any>(), nullptr) << "wire 'b' is declared without an initializer";
}

TEST_F(RealBitSelect, NoProcesses) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getProcesses() == nullptr || top->getProcesses()->empty());
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
