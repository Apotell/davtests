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
//   - design has module work@top
//   - module has exactly 3 nets: 'a' (real), 'b' (wire[3:0]), 'c' (wire)
//   - 'a' typespec → RealTypespec; initial value vpiRealConst "0.5"
//   - 'b' and 'c' net types are vpiWire; no initial values
//   - 1 ContAssign: LHS RefObj "c" resolves to net 'c', RHS = BitSelect "b[a]"
//   - BitSelect prefix RefObj "b" resolves to net 'b'
//   - BitSelect index RefObj "a" resolves to the real Net 'a'
//   - work@top has no processes
//
// Not checked:
//   - Surelog doesn't flag the illegal real-typed bit-select index

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

class RealBitSelectIdx : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "6.12--real_bit_select_idx.hlc"});

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

TEST_F(RealBitSelectIdx, ModuleExists) {
  ASSERT_NE(uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules()), nullptr);
}

// ---------------------------------------------------------------------------
// Net declarations — real 'a', wire[3:0] 'b', wire 'c'
// ---------------------------------------------------------------------------
TEST_F(RealBitSelectIdx, ThreeNetsExist) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 3u)
      << "expected nets 'a' (real), 'b' (wire[3:0]), 'c' (wire)";
}

TEST_F(RealBitSelectIdx, ANetTypespecIsReal) {
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

TEST_F(RealBitSelectIdx, ANetInitialValueIsHalf) {
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

TEST_F(RealBitSelectIdx, BNetIsWire) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const b = uhdm::findByName<uhdm::Net>("b", top->getNets());
  ASSERT_NE(b, nullptr) << "net 'b' not found";
  EXPECT_EQ(b->getNetType(), vpiWire);
}

TEST_F(RealBitSelectIdx, CNetIsWire) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const c = uhdm::findByName<uhdm::Net>("c", top->getNets());
  ASSERT_NE(c, nullptr) << "net 'c' not found";
  EXPECT_EQ(c->getNetType(), vpiWire);
}

// ---------------------------------------------------------------------------
// Continuous assignment — assign c = b[a]
// ---------------------------------------------------------------------------
TEST_F(RealBitSelectIdx, ContAssignExists) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  EXPECT_EQ(top->getContAssigns()->size(), 1u);
}

TEST_F(RealBitSelectIdx, ContAssignLhsIsC) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);

  const uhdm::RefObj *const lhs =
      top->getContAssigns()->at(0)->getLhs<uhdm::RefObj>();
  ASSERT_NE(lhs, nullptr) << "ContAssign LHS is not a RefObj";
  EXPECT_EQ(lhs->getName(), "c");
}

TEST_F(RealBitSelectIdx, ContAssignRhsIsBitSelect) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);

  const uhdm::BitSelect *const rhs =
      top->getContAssigns()->at(0)->getRhs<uhdm::BitSelect>();
  ASSERT_NE(rhs, nullptr) << "ContAssign RHS is not a BitSelect";
  EXPECT_EQ(rhs->getName(), "b[a]");
}

// ---------------------------------------------------------------------------
// BitSelect internals — prefix is logic 'b', index is real RefObj 'a'
// ---------------------------------------------------------------------------
TEST_F(RealBitSelectIdx, BitSelectPrefixIsRefObjB) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);

  const uhdm::BitSelect *const rhs =
      top->getContAssigns()->at(0)->getRhs<uhdm::BitSelect>();
  ASSERT_NE(rhs, nullptr);

  const uhdm::RefObj *const prefix = rhs->getPrefix<uhdm::RefObj>();
  ASSERT_NE(prefix, nullptr) << "BitSelect prefix is not a RefObj";
  EXPECT_EQ(prefix->getName(), "b");
}

TEST_F(RealBitSelectIdx, BitSelectPrefixResolvesToNetB) {
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
  EXPECT_EQ(net->getName(), "b");
}

TEST_F(RealBitSelectIdx, BitSelectIndexIsRefObjA) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);

  const uhdm::BitSelect *const rhs =
      top->getContAssigns()->at(0)->getRhs<uhdm::BitSelect>();
  ASSERT_NE(rhs, nullptr);

  // Index is a real variable — illegal in SV but Surelog records it as RefObj
  const uhdm::RefObj *const idx = rhs->getIndex<uhdm::RefObj>();
  ASSERT_NE(idx, nullptr) << "BitSelect index is not a RefObj";
  EXPECT_EQ(idx->getName(), "a");
}

TEST_F(RealBitSelectIdx, BitSelectIndexResolvesToRealNet) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);

  const uhdm::BitSelect *const rhs =
      top->getContAssigns()->at(0)->getRhs<uhdm::BitSelect>();
  ASSERT_NE(rhs, nullptr);
  const uhdm::RefObj *const idx = rhs->getIndex<uhdm::RefObj>();
  ASSERT_NE(idx, nullptr);

  const uhdm::Net *const net = idx->getActual<uhdm::Net>();
  ASSERT_NE(net, nullptr) << "BitSelect index RefObj does not resolve to a Net";
  EXPECT_EQ(net->getName(), "a");

  const uhdm::RefTypespec *const rts = net->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<uhdm::RealTypespec>(), nullptr)
      << "index net 'a' must be real-typed — it is the illegal real index";
}

TEST_F(RealBitSelectIdx, ContAssignLhsResolvesToNetC) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  const uhdm::RefObj *const lhs =
      top->getContAssigns()->at(0)->getLhs<uhdm::RefObj>();
  ASSERT_NE(lhs, nullptr);
  const uhdm::Net *const net = lhs->getActual<uhdm::Net>();
  ASSERT_NE(net, nullptr)
      << "ContAssign LHS RefObj 'c' should resolve to the formally declared net 'c'";
  EXPECT_EQ(net->getName(), "c");
}

TEST_F(RealBitSelectIdx, BNetHasNoInitialValue) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const b = uhdm::findByName<uhdm::Net>("b", top->getNets());
  ASSERT_NE(b, nullptr);
  EXPECT_EQ(b->getValue<uhdm::Any>(), nullptr)
      << "wire 'b' is declared without an initializer";
}

TEST_F(RealBitSelectIdx, CNetHasNoInitialValue) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const c = uhdm::findByName<uhdm::Net>("c", top->getNets());
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getValue<uhdm::Any>(), nullptr)
      << "wire 'c' is declared without an initializer";
}

TEST_F(RealBitSelectIdx, NoProcesses) {
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
