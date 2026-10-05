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

// Tests for tests/NetLValue/dut.sv:
//
//   module t;
//      typedef struct { int x; } S;
//      S[1:0] s;
//      int y;
//
//      always_comb begin
//        y = s[0].x;
//        s[1].x = 0;
//      end
//   endmodule // t
//
// What to check and why (IEEE 1800-2023):
//   - Sec 7.2: "typedef struct { int x; } S;" declares an unpacked
//     structure type (no 'packed' keyword) with one int member 'x'.
//   - Sec 7.4.1: "Packed arrays can be made of only the single bit data
//     types (bit, logic, reg), enumerated types, and recursively other
//     packed arrays and packed structures." "S[1:0] s;" places a packed
//     dimension on an UNPACKED structure, which is illegal and must be
//     diagnosed (line 3).
//   - Sec 6.8: "S[1:0] s;" and "int y;" have no net-type keyword, so both
//     are variables, never nets -- the left-hand side "s[1].x" of the
//     procedural assignment is therefore a variable lvalue (Sec 10.4: a
//     procedural assignment's lvalue shall be a variable).
//   - Sec 9.2.2.2: always_comb procedure; Sec 10.4.1: both '=' statements
//     are blocking procedural assignments.
//   - Sec 7.2.1/11.5.1: "s[0].x" / "s[1].x" are a member select of an
//     element select of 's', each resolving to member 'x'.
//
// What is NOT checked and why:
//   - Simulation values / sensitivity list: no elaboration requested.
//   - The exact HLC error code for the illegal packed dimension is
//     implementation-specific; any of HLC's packed-array diagnostics on
//     line 3 is accepted.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/always.h>
#include <hldb/array_typespec.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/bit_select.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/int_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/struct.h>
#include <hldb/struct_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/typedef.h>
#include <hldb/typedef_typespec.h>
#include <hldb/typespec_member.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class NetLValueTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "NetLValue.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getT() { return hldb::findByName<hldb::Module>("t", m_design->getAllModules()); }

  static const hldb::Variable *getVar(std::string_view name) {
    const hldb::Module *const t = getT();
    if (t == nullptr || t->getVariables() == nullptr) return nullptr;
    return hldb::findByName<hldb::Variable>(name, t->getVariables());
  }

  static const hldb::Typedef *getTypedefS() {
    const hldb::Module *const t = getT();
    if (t == nullptr || t->getTypedefs() == nullptr) return nullptr;
    return hldb::findByName<hldb::Typedef>("S", t->getTypedefs());
  }

  static const hldb::Begin *getAlwaysBody() {
    const hldb::Module *const t = getT();
    if (t == nullptr || t->getProcesses() == nullptr || t->getProcesses()->size() != 1) return nullptr;
    const hldb::Always *const a = any_cast<hldb::Always>(t->getProcesses()->at(0));
    if (a == nullptr) return nullptr;
    return any_cast<hldb::Begin>(a->getStmt());
  }

  static const hldb::Assignment *getAssign(size_t idx) {
    const hldb::Begin *const b = getAlwaysBody();
    if (b == nullptr || b->getStmts() == nullptr || b->getStmts()->size() <= idx) return nullptr;
    return any_cast<hldb::Assignment>(b->getStmts()->at(idx));
  }

  // Checks that 'ref' is s[<index>].x
  static void checkMemberOfElement(const hldb::RefObj *ref, const char *index) {
    ASSERT_NE(ref, nullptr);
    ASSERT_NE(ref->getPathElems(), nullptr);
    ASSERT_EQ(ref->getPathElems()->size(), 2u);
    const hldb::BitSelect *const bs = any_cast<hldb::BitSelect>(ref->getPathElems()->at(0));
    ASSERT_NE(bs, nullptr) << "first path element must be the element select s[" << index << "]";
    const hldb::RefObj *const prefix = any_cast<hldb::RefObj>(bs->getPrefix());
    ASSERT_NE(prefix, nullptr);
    EXPECT_EQ(prefix->getName(), std::string_view("s"));
    ASSERT_NE(prefix->getActual(), nullptr);
    EXPECT_EQ(prefix->getActual()->getAnyType(), hldb::AnyType::Variable);
    const hldb::Constant *const idx = any_cast<hldb::Constant>(bs->getIndex());
    ASSERT_NE(idx, nullptr);
    EXPECT_EQ(idx->getDecompile(), std::string_view(index));
    const hldb::RefObj *const member = any_cast<hldb::RefObj>(ref->getPathElems()->at(1));
    ASSERT_NE(member, nullptr);
    EXPECT_EQ(member->getName(), std::string_view("x"));
    ASSERT_NE(member->getActual(), nullptr);
    EXPECT_EQ(member->getActual()->getAnyType(), hldb::AnyType::TypespecMember);
  }
};

// ---------------------------------------------------------------------------
// Existence
// ---------------------------------------------------------------------------

TEST_F(NetLValueTest, ModuleTExists) { ASSERT_NE(getT(), nullptr) << "module 't' not found"; }

// ---------------------------------------------------------------------------
// typedef struct { int x; } S;  (Sec 7.2, unpacked)
// ---------------------------------------------------------------------------

TEST_F(NetLValueTest, TypedefSIsUnpackedStructWithIntMemberX) {
  const hldb::Typedef *const td = getTypedefS();
  ASSERT_NE(td, nullptr);
  ASSERT_NE(td->getAlias(), nullptr);
  const hldb::StructTypespec *const st = td->getAlias()->getActual<hldb::StructTypespec>();
  ASSERT_NE(st, nullptr);
  const hldb::Struct *const s = st->getStruct();
  ASSERT_NE(s, nullptr);
  EXPECT_FALSE(s->getPacked()) << "no 'packed' keyword -> unpacked structure";
  ASSERT_NE(s->getMembers(), nullptr);
  ASSERT_EQ(s->getMembers()->size(), 1u);
  const hldb::TypespecMember *const x = s->getMembers()->at(0);
  EXPECT_EQ(x->getName(), std::string_view("x"));
  ASSERT_NE(x->getTypespec(), nullptr);
  EXPECT_NE(x->getTypespec()->getActual<hldb::IntTypespec>(), nullptr);
}

// ---------------------------------------------------------------------------
// S[1:0] s;  -- Sec 7.4.1 illegal packed dimension on unpacked struct
// ---------------------------------------------------------------------------

TEST_F(NetLValueTest, PackedDimensionOnUnpackedStructIsDiagnosed) {
  const bool found = (findError(ErrorDefinition::COMP_ILLEGAL_PACKED_ARRAY_TYPE, 3) != nullptr) ||
                     (findError(ErrorDefinition::COMP_UNPACKED_IN_PACKED, 3) != nullptr) ||
                     (findError(ErrorDefinition::HLDB_ILLEGAL_PACKED_DIMENSION, 3) != nullptr);
  EXPECT_TRUE(found) << "Sec 7.4.1: packed arrays can only be made of single-bit types, enums, packed arrays "
                        "and packed structures; 'S[1:0]' with unpacked S must be an error";
}

TEST_F(NetLValueTest, SIsVariableNotNet) {
  const hldb::Module *const t = getT();
  ASSERT_NE(t, nullptr);
  EXPECT_NE(getVar("s"), nullptr) << "Sec 6.8: no net-type keyword -> variable";
  if (t->getNets() != nullptr) {
    EXPECT_EQ(hldb::findByName<hldb::Net>("s", t->getNets()), nullptr);
  }
}

TEST_F(NetLValueTest, STypespecIsPackedArray1To0OfS) {
  const hldb::Variable *const s = getVar("s");
  ASSERT_NE(s, nullptr);
  ASSERT_NE(s->getTypespec(), nullptr);
  const hldb::ArrayTypespec *const at = s->getTypespec()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  EXPECT_TRUE(at->getPacked()) << "[1:0] before the identifier is a packed dimension";
  const hldb::Range *const r = at->getRange();
  ASSERT_NE(r, nullptr);
  const hldb::Constant *const left = any_cast<hldb::Constant>(r->getLeftExpr());
  const hldb::Constant *const right = any_cast<hldb::Constant>(r->getRightExpr());
  ASSERT_NE(left, nullptr);
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(left->getDecompile(), std::string_view("1"));
  EXPECT_EQ(right->getDecompile(), std::string_view("0"));
  ASSERT_NE(at->getElemTypespec(), nullptr);
  const hldb::TypedefTypespec *const elem = at->getElemTypespec()->getActual<hldb::TypedefTypespec>();
  ASSERT_NE(elem, nullptr);
  EXPECT_EQ(elem->getName(), std::string_view("S"));
}

// ---------------------------------------------------------------------------
// int y;
// ---------------------------------------------------------------------------

TEST_F(NetLValueTest, YIsIntVariableNotNet) {
  const hldb::Module *const t = getT();
  ASSERT_NE(t, nullptr);
  const hldb::Variable *const y = getVar("y");
  ASSERT_NE(y, nullptr);
  ASSERT_NE(y->getTypespec(), nullptr);
  const hldb::IntTypespec *const it = y->getTypespec()->getActual<hldb::IntTypespec>();
  ASSERT_NE(it, nullptr);
  EXPECT_TRUE(it->getSigned()) << "Sec 6.11: int is signed";
  if (t->getNets() != nullptr) {
    EXPECT_EQ(hldb::findByName<hldb::Net>("y", t->getNets()), nullptr);
  }
}

TEST_F(NetLValueTest, ModuleHasNoNets) {
  const hldb::Module *const t = getT();
  ASSERT_NE(t, nullptr);
  if (t->getNets() != nullptr) {
    EXPECT_EQ(t->getNets()->size(), 0u) << "Sec 6.8: no net declarations appear in module t";
  }
}

// ---------------------------------------------------------------------------
// always_comb begin ... end
// ---------------------------------------------------------------------------

TEST_F(NetLValueTest, OneAlwaysCombProcess) {
  const hldb::Module *const t = getT();
  ASSERT_NE(t, nullptr);
  ASSERT_NE(t->getProcesses(), nullptr);
  ASSERT_EQ(t->getProcesses()->size(), 1u);
  const hldb::Always *const a = any_cast<hldb::Always>(t->getProcesses()->at(0));
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(a->getAlwaysType(), vpiAlwaysComb);
  const hldb::Begin *const b = any_cast<hldb::Begin>(a->getStmt());
  ASSERT_NE(b, nullptr);
  ASSERT_NE(b->getStmts(), nullptr);
  EXPECT_EQ(b->getStmts()->size(), 2u);
}

TEST_F(NetLValueTest, FirstAssignIsBlockingYEqualsS0X) {
  const hldb::Assignment *const as = getAssign(0);
  ASSERT_NE(as, nullptr);
  EXPECT_TRUE(as->getBlocking());
  const hldb::RefObj *const lhs = any_cast<hldb::RefObj>(as->getLhs());
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), std::string_view("y"));
  ASSERT_NE(lhs->getActual(), nullptr);
  EXPECT_EQ(lhs->getActual()->getAnyType(), hldb::AnyType::Variable);
  checkMemberOfElement(any_cast<hldb::RefObj>(as->getRhs()), "0");
}

TEST_F(NetLValueTest, SecondAssignIsBlockingS1XEqualsZero) {
  const hldb::Assignment *const as = getAssign(1);
  ASSERT_NE(as, nullptr);
  EXPECT_TRUE(as->getBlocking());
  checkMemberOfElement(any_cast<hldb::RefObj>(as->getLhs()), "1");
  const hldb::Constant *const rhs = any_cast<hldb::Constant>(as->getRhs());
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), std::string_view("0"));
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
