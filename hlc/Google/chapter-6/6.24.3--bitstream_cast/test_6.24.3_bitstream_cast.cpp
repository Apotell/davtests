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

// Validates the UHDM graph for a module using a bitstream cast:
//   module top();
//     struct packed {logic [7:0] a; logic [7:0] b; logic [15:0] c;} s;
//     integer a = integer'(s);
//   endmodule
//
// Checked:
//   - design has module work@top
//   - module has exactly 2 nets: 's' (packed StructTypespec) and 'a' (IntegerTypespec)
//   - struct 's' is packed, has 3 members (a, b, c), all with LogicTypespec
//   - 's' has no initial value
//   - 'a' vpiValue = vpiCastOp Operation; cast typespec → IntegerTypespec
//   - cast operand = RefObj "s" → Net 's'
//   - work@top has no continuous assignments
//   - work@top has no processes
//
// Not checked:
//   - struct member bit widths (a, b → [7:0]; c → [15:0])
//   - actual result of the bitstream cast (runtime-only)

#include <Surelog/Common/Session.h>
#include <Surelog/SourceCompile/Compiler.h>
#include <Surelog/Tests/Test.h>

#include <uhdm/Utils.h>
#include <uhdm/design.h>
#include <uhdm/integer_typespec.h>
#include <uhdm/logic_typespec.h>
#include <uhdm/module.h>
#include <uhdm/net.h>
#include <uhdm/operation.h>
#include <uhdm/ref_obj.h>
#include <uhdm/ref_typespec.h>
#include <uhdm/struct_typespec.h>
#include <uhdm/typespec_member.h>
#include <uhdm/vpi_user.h>

namespace SURELOG {

class BitstreamCast : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "6.24.3--bitstream_cast.hlc"});

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

TEST_F(BitstreamCast, ModuleExists) {
  ASSERT_NE(uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules()), nullptr);
}

// ---------------------------------------------------------------------------
// No processes — both declarations are module-level Nets
// ---------------------------------------------------------------------------
TEST_F(BitstreamCast, NoProcesses) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getProcesses() == nullptr || top->getProcesses()->empty());
}

// ---------------------------------------------------------------------------
// Net "s" → StructTypespec (packed, 3 members: a, b, c)
// ---------------------------------------------------------------------------
TEST_F(BitstreamCast, NetSIsPackedStruct) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const s = uhdm::findByName<uhdm::Net>("s", top->getNets());
  ASSERT_NE(s, nullptr);
  const uhdm::StructTypespec *const structTs =
      s->getTypespec()->getActual<uhdm::StructTypespec>();
  ASSERT_NE(structTs, nullptr);
  EXPECT_TRUE(structTs->getPacked());
}

TEST_F(BitstreamCast, StructHasThreeMembers) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const s = uhdm::findByName<uhdm::Net>("s", top->getNets());
  ASSERT_NE(s, nullptr);
  const uhdm::StructTypespec *const structTs =
      s->getTypespec()->getActual<uhdm::StructTypespec>();
  ASSERT_NE(structTs, nullptr);
  ASSERT_NE(structTs->getMembers(), nullptr);
  EXPECT_EQ(structTs->getMembers()->size(), 3u);
  EXPECT_EQ(structTs->getMembers()->at(0)->getName(), "a");
  EXPECT_EQ(structTs->getMembers()->at(1)->getName(), "b");
  EXPECT_EQ(structTs->getMembers()->at(2)->getName(), "c");
}

TEST_F(BitstreamCast, StructMembersHaveLogicTypespec) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const s = uhdm::findByName<uhdm::Net>("s", top->getNets());
  ASSERT_NE(s, nullptr);
  const uhdm::StructTypespec *const structTs =
      s->getTypespec()->getActual<uhdm::StructTypespec>();
  ASSERT_NE(structTs, nullptr);
  for (const auto *member : *structTs->getMembers()) {
    EXPECT_NE(member->getTypespec()->getActual<uhdm::LogicTypespec>(), nullptr)
        << "struct member " << member->getName() << " should be LogicTypespec";
  }
}

// ---------------------------------------------------------------------------
// Net "a" → IntegerTypespec (integer keyword — distinct from IntTypespec/int)
// ---------------------------------------------------------------------------
TEST_F(BitstreamCast, NetAIsIntegerType) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const a = uhdm::findByName<uhdm::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr);
  EXPECT_NE(a->getTypespec()->getActual<uhdm::IntegerTypespec>(), nullptr)
      << "integer keyword maps to IntegerTypespec (not IntTypespec which is for int)";
}

// ---------------------------------------------------------------------------
// Net "a" vpiValue = Operation(vpiCastOp=67) — integer'(s)
// ---------------------------------------------------------------------------
TEST_F(BitstreamCast, NetAValueIsCastOperation) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const a = uhdm::findByName<uhdm::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr);
  const uhdm::Operation *const castOp = a->getValue<uhdm::Operation>();
  ASSERT_NE(castOp, nullptr);
  EXPECT_EQ(castOp->getOpType(), vpiCastOp);
}

TEST_F(BitstreamCast, CastTypespecIsIntegerTypespec) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const a = uhdm::findByName<uhdm::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr);
  const uhdm::Operation *const castOp = a->getValue<uhdm::Operation>();
  ASSERT_NE(castOp, nullptr);
  const uhdm::RefTypespec *const rts = castOp->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<uhdm::IntegerTypespec>(), nullptr)
      << "integer'(...) cast target type is IntegerTypespec";
}

TEST_F(BitstreamCast, CastOperandIsRefToNetS) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const a = uhdm::findByName<uhdm::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr);
  const uhdm::Operation *const castOp = a->getValue<uhdm::Operation>();
  ASSERT_NE(castOp, nullptr);
  ASSERT_NE(castOp->getOperands(), nullptr);
  ASSERT_EQ(castOp->getOperands()->size(), 1u);
  const uhdm::RefObj *const s =
      any_cast<uhdm::RefObj>(castOp->getOperands()->at(0));
  ASSERT_NE(s, nullptr);
  EXPECT_EQ(s->getName(), "s");
  EXPECT_NE(s->getActual<uhdm::Net>(), nullptr);
}

// ---------------------------------------------------------------------------
// Structural completeness
// ---------------------------------------------------------------------------
TEST_F(BitstreamCast, TwoNetsExist) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 2u) << "expected nets 's' (struct) and 'a' (integer)";
}

TEST_F(BitstreamCast, NetSHasNoInitialValue) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const s = uhdm::findByName<uhdm::Net>("s", top->getNets());
  ASSERT_NE(s, nullptr);
  EXPECT_EQ(s->getValue<uhdm::Any>(), nullptr)
      << "struct 's' is declared without an initializer";
}

TEST_F(BitstreamCast, NoContAssigns) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty())
      << "integer a = integer'(s) stores the cast as vpiValue, not a ContAssign";
}

}  // namespace SURELOG

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
