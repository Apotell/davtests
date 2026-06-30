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
  ASSERT_NE(hldb::findByName<hldb::Module>("work@top", m_design->getAllModules()), nullptr);
}

// ---------------------------------------------------------------------------
// Net declarations — real 'a', wire[3:0] 'b', wire 'c'
// ---------------------------------------------------------------------------
TEST_F(RealBitSelectIdx, ThreeNetsExist) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 3u)
      << "expected nets 'a' (real), 'b' (wire[3:0]), 'c' (wire)";
}

TEST_F(RealBitSelectIdx, ANetTypespecIsReal) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const a = hldb::findByName<hldb::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr) << "net 'a' not found";

  const hldb::RefTypespec *const rts = a->getTypespec();
  ASSERT_NE(rts, nullptr) << "net 'a' has no typespec";
  EXPECT_NE(rts->getActual<hldb::RealTypespec>(), nullptr)
      << "net 'a' typespec should resolve to RealTypespec";
}

TEST_F(RealBitSelectIdx, ANetInitialValueIsHalf) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const a = hldb::findByName<hldb::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr);
  const hldb::Constant *const init = a->getValue<hldb::Constant>();
  ASSERT_NE(init, nullptr);
  EXPECT_EQ(init->getConstType(), vpiRealConst);
  EXPECT_EQ(init->getDecompile(), "0.5");
}

TEST_F(RealBitSelectIdx, BNetIsWire) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const b = hldb::findByName<hldb::Net>("b", top->getNets());
  ASSERT_NE(b, nullptr) << "net 'b' not found";
  EXPECT_EQ(b->getNetType(), vpiWire);
}

TEST_F(RealBitSelectIdx, CNetIsWire) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const c = hldb::findByName<hldb::Net>("c", top->getNets());
  ASSERT_NE(c, nullptr) << "net 'c' not found";
  EXPECT_EQ(c->getNetType(), vpiWire);
}

// ---------------------------------------------------------------------------
// Continuous assignment — assign c = b[a]
// ---------------------------------------------------------------------------
TEST_F(RealBitSelectIdx, ContAssignExists) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  EXPECT_EQ(top->getContAssigns()->size(), 1u);
}

TEST_F(RealBitSelectIdx, ContAssignLhsIsC) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);

  const hldb::RefObj *const lhs =
      top->getContAssigns()->at(0)->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr) << "ContAssign LHS is not a RefObj";
  EXPECT_EQ(lhs->getName(), "c");
}

TEST_F(RealBitSelectIdx, ContAssignRhsIsBitSelect) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);

  const hldb::BitSelect *const rhs =
      top->getContAssigns()->at(0)->getRhs<hldb::BitSelect>();
  ASSERT_NE(rhs, nullptr) << "ContAssign RHS is not a BitSelect";
  EXPECT_EQ(rhs->getName(), "b[a]");
}

// ---------------------------------------------------------------------------
// BitSelect internals — prefix is logic 'b', index is real RefObj 'a'
// ---------------------------------------------------------------------------
TEST_F(RealBitSelectIdx, BitSelectPrefixIsRefObjB) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);

  const hldb::BitSelect *const rhs =
      top->getContAssigns()->at(0)->getRhs<hldb::BitSelect>();
  ASSERT_NE(rhs, nullptr);

  const hldb::RefObj *const prefix = rhs->getPrefix<hldb::RefObj>();
  ASSERT_NE(prefix, nullptr) << "BitSelect prefix is not a RefObj";
  EXPECT_EQ(prefix->getName(), "b");
}

TEST_F(RealBitSelectIdx, BitSelectPrefixResolvesToNetB) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);

  const hldb::BitSelect *const rhs =
      top->getContAssigns()->at(0)->getRhs<hldb::BitSelect>();
  ASSERT_NE(rhs, nullptr);
  const hldb::RefObj *const prefix = rhs->getPrefix<hldb::RefObj>();
  ASSERT_NE(prefix, nullptr);

  const hldb::Net *const net = prefix->getActual<hldb::Net>();
  ASSERT_NE(net, nullptr) << "BitSelect prefix does not resolve to a Net";
  EXPECT_EQ(net->getName(), "b");
}

TEST_F(RealBitSelectIdx, BitSelectIndexIsRefObjA) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);

  const hldb::BitSelect *const rhs =
      top->getContAssigns()->at(0)->getRhs<hldb::BitSelect>();
  ASSERT_NE(rhs, nullptr);

  // Index is a real variable — illegal in SV but Surelog records it as RefObj
  const hldb::RefObj *const idx = rhs->getIndex<hldb::RefObj>();
  ASSERT_NE(idx, nullptr) << "BitSelect index is not a RefObj";
  EXPECT_EQ(idx->getName(), "a");
}

TEST_F(RealBitSelectIdx, BitSelectIndexResolvesToRealNet) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);

  const hldb::BitSelect *const rhs =
      top->getContAssigns()->at(0)->getRhs<hldb::BitSelect>();
  ASSERT_NE(rhs, nullptr);
  const hldb::RefObj *const idx = rhs->getIndex<hldb::RefObj>();
  ASSERT_NE(idx, nullptr);

  const hldb::Net *const net = idx->getActual<hldb::Net>();
  ASSERT_NE(net, nullptr) << "BitSelect index RefObj does not resolve to a Net";
  EXPECT_EQ(net->getName(), "a");

  const hldb::RefTypespec *const rts = net->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::RealTypespec>(), nullptr)
      << "index net 'a' must be real-typed — it is the illegal real index";
}

TEST_F(RealBitSelectIdx, ContAssignLhsResolvesToNetC) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  const hldb::RefObj *const lhs =
      top->getContAssigns()->at(0)->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  const hldb::Net *const net = lhs->getActual<hldb::Net>();
  ASSERT_NE(net, nullptr)
      << "ContAssign LHS RefObj 'c' should resolve to the formally declared net 'c'";
  EXPECT_EQ(net->getName(), "c");
}

TEST_F(RealBitSelectIdx, BNetHasNoInitialValue) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const b = hldb::findByName<hldb::Net>("b", top->getNets());
  ASSERT_NE(b, nullptr);
  EXPECT_EQ(b->getValue<hldb::Any>(), nullptr)
      << "wire 'b' is declared without an initializer";
}

TEST_F(RealBitSelectIdx, CNetHasNoInitialValue) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const c = hldb::findByName<hldb::Net>("c", top->getNets());
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getValue<hldb::Any>(), nullptr)
      << "wire 'c' is declared without an initializer";
}

TEST_F(RealBitSelectIdx, NoProcesses) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getProcesses() == nullptr || top->getProcesses()->empty());
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
