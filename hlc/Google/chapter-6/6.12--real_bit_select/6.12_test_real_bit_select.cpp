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
//   - Surelog doesn't flag the illegal bit-select on real

#include <Surelog/Common/Session.h>
#include <Surelog/SourceCompile/Compiler.h>
#include <Surelog/Tests/Test.h>

#include <uhdm/Utils.h>
#include <uhdm/bit_select.h>
#include <uhdm/constant.h>
#include <uhdm/cont_assign.h>
#include <uhdm/design.h>
#include <uhdm/module.h>
#include <uhdm/net.h>
#include <uhdm/real_typespec.h>
#include <uhdm/ref_obj.h>
#include <uhdm/ref_typespec.h>
#include <uhdm/vpi_user.h>

namespace SURELOG {

class RealBitSelect : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "6.12--real_bit_select.hlc"});

    ASSERT_NE(m_session, nullptr) << "Session is null";
    ASSERT_NE(m_compiler, nullptr) << "Compiler is null";
    ASSERT_NE(m_design, nullptr) << "Design is null";
  }

  static void TearDownTestSuite() {
    m_design = nullptr;
    delete m_compiler;
    m_compiler = nullptr;
    delete m_session;
    m_session = nullptr;
  }
};

TEST_F(RealBitSelect, ModuleExists) {
  ASSERT_NE(uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules()), nullptr);
}

// ---------------------------------------------------------------------------
// Net declarations — real 'a' and wire 'b'
// ---------------------------------------------------------------------------
TEST_F(RealBitSelect, TwoNetsExist) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 2u) << "expected nets 'a' (real) and 'b' (wire)";
}

TEST_F(RealBitSelect, ANetTypespecIsReal) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const a = uhdm::findByName<uhdm::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr) << "net 'a' not found";

  const uhdm::RefTypespec *const rts = a->getTypespec();
  ASSERT_NE(rts, nullptr) << "net 'a' has no typespec";
  EXPECT_NE(rts->getActual<uhdm::RealTypespec>(), nullptr)
      << "net 'a' typespec should resolve to RealTypespec";
}

TEST_F(RealBitSelect, ANetInitialValueIsHalf) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const a = uhdm::findByName<uhdm::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr);
  const uhdm::Constant *const init = a->getValue<uhdm::Constant>();
  ASSERT_NE(init, nullptr);
  EXPECT_EQ(init->getConstType(), vpiRealConst);
  EXPECT_EQ(init->getDecompile(), "0.5");
}

TEST_F(RealBitSelect, BNetIsWire) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const b = uhdm::findByName<uhdm::Net>("b", top->getNets());
  ASSERT_NE(b, nullptr) << "net 'b' not found";
  EXPECT_EQ(b->getNetType(), vpiWire);
}

// ---------------------------------------------------------------------------
// Continuous assignment — assign b = a[2]
// ---------------------------------------------------------------------------
TEST_F(RealBitSelect, ContAssignExists) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  EXPECT_EQ(top->getContAssigns()->size(), 1u);
}

TEST_F(RealBitSelect, ContAssignLhsIsB) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);

  const uhdm::RefObj *const lhs =
      top->getContAssigns()->at(0)->getLhs<uhdm::RefObj>();
  ASSERT_NE(lhs, nullptr) << "ContAssign LHS is not a RefObj";
  EXPECT_EQ(lhs->getName(), "b");
}

TEST_F(RealBitSelect, ContAssignRhsIsBitSelect) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);

  const uhdm::BitSelect *const rhs =
      top->getContAssigns()->at(0)->getRhs<uhdm::BitSelect>();
  ASSERT_NE(rhs, nullptr) << "ContAssign RHS is not a BitSelect";
  EXPECT_EQ(rhs->getName(), "a[2]");
}

// ---------------------------------------------------------------------------
// BitSelect internals — prefix is real 'a', index is Constant 2
// ---------------------------------------------------------------------------
TEST_F(RealBitSelect, BitSelectPrefixIsRefObjA) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);

  const uhdm::BitSelect *const rhs =
      top->getContAssigns()->at(0)->getRhs<uhdm::BitSelect>();
  ASSERT_NE(rhs, nullptr);

  const uhdm::RefObj *const prefix = rhs->getPrefix<uhdm::RefObj>();
  ASSERT_NE(prefix, nullptr) << "BitSelect prefix is not a RefObj";
  EXPECT_EQ(prefix->getName(), "a");
}

TEST_F(RealBitSelect, BitSelectPrefixResolvesToRealNet) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);

  const uhdm::BitSelect *const rhs =
      top->getContAssigns()->at(0)->getRhs<uhdm::BitSelect>();
  ASSERT_NE(rhs, nullptr);
  const uhdm::RefObj *const prefix = rhs->getPrefix<uhdm::RefObj>();
  ASSERT_NE(prefix, nullptr);

  const uhdm::Net *const net = prefix->getActual<uhdm::Net>();
  ASSERT_NE(net, nullptr) << "BitSelect prefix does not resolve to a Net";
  const uhdm::RefTypespec *const rts = net->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<uhdm::RealTypespec>(), nullptr)
      << "bit-selected prefix 'a' should be a real-typed net";
}

TEST_F(RealBitSelect, BitSelectIndexIsConstant2) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);

  const uhdm::BitSelect *const rhs =
      top->getContAssigns()->at(0)->getRhs<uhdm::BitSelect>();
  ASSERT_NE(rhs, nullptr);

  const uhdm::Constant *const idx = rhs->getIndex<uhdm::Constant>();
  ASSERT_NE(idx, nullptr) << "BitSelect index is not a Constant";
  EXPECT_EQ(idx->getDecompile(), "2");
}

TEST_F(RealBitSelect, ContAssignLhsResolvesToNetB) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  const uhdm::RefObj *const lhs =
      top->getContAssigns()->at(0)->getLhs<uhdm::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_NE(lhs->getActual<uhdm::Net>(), nullptr)
      << "ContAssign LHS RefObj 'b' should resolve to the formally declared net 'b'";
}

TEST_F(RealBitSelect, BNetHasNoInitialValue) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const b = uhdm::findByName<uhdm::Net>("b", top->getNets());
  ASSERT_NE(b, nullptr);
  EXPECT_EQ(b->getValue<uhdm::Any>(), nullptr)
      << "wire 'b' is declared without an initializer";
}

TEST_F(RealBitSelect, NoProcesses) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getProcesses() == nullptr || top->getProcesses()->empty());
}

}  // namespace SURELOG

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
