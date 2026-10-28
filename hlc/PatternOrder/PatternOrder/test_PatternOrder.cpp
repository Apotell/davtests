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

// Validates the HLDB model built for tests/PatternOrder/dut.sv:
//
//   module top();
//      struct {
//     int A;
//     struct {
//       logic B;
//       real C;
//     } BC1, BC2;
//   } ABC = '{BC1: '{1'b1, 1.0}, int: 0, BC2: '{default: 0}};
//
//   endmodule : top
//
// The fixture is a structure assignment pattern (10.9.2) mixing member keys
// (BC1, BC2), a type key (int) and a nested default, written out of member
// order. Because the items are keyed rather than positional, each key must
// name the right member.
//
// What is checked, and why:
//   - ABC is a variable of an unpacked structure (7.2) with members A, BC1
//     and BC2 in declaration order; BC1 and BC2 are structures with members
//     B (logic) and C (real)
//   - ABC's initializer is an assignment pattern of 3 items in source order
//   - the keys BC1 and BC2 bind to the structure members BC1 and BC2
//     (10.9.2)
//   - the key 'int' is a type key naming int (10.9.2)
//   - BC1's value is a positional pattern of 1'b1 and 1.0; BC2's value is a
//     pattern with one 'default: 0' item
//   - no fatal, syntax or error diagnostic: the file is legal
//
// KNOWN COMPILER BUG (structure pattern member keys not bound), not a defect
// in this test: HLC leaves the keys BC1 and BC2 of
// '{BC1: '{1'b1, 1.0}, int: 0, BC2: '{default: 0}} unbound and only warns
// "Failed to bind", although 10.9.2 says "A member:value specifies an
// explicit value for a named member of the structure."
// KeysBc1AndBc2BindToMembers is expected to fail until HLC is fixed; it is
// intentionally not skipped or relaxed.
//
// What is NOT checked, and why:
//   - The resulting member values: reducing the pattern is elaboration, and
//     no .hlc in this suite elaborates.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/int_typespec.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/struct.h>
#include <hldb/struct_typespec.h>
#include <hldb/tagged_pattern.h>
#include <hldb/typespec_member.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <string_view>

namespace hlc {

class PatternOrderTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "PatternOrder.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Variable *getAbc() {
    const hldb::Module *const m = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
    if (m == nullptr) return nullptr;
    return hldb::findByName<hldb::Variable>("ABC", m->getVariables());
  }

  static const hldb::Struct *getStruct(const hldb::RefTypespec *rt) {
    if (rt == nullptr) return nullptr;
    const hldb::StructTypespec *const st = rt->getActual<hldb::StructTypespec>();
    return (st == nullptr) ? nullptr : st->getStruct();
  }

  static const hldb::TaggedPattern *getItem(size_t i) {
    const hldb::Variable *const v = getAbc();
    if (v == nullptr) return nullptr;
    const hldb::Operation *const pat = any_cast<hldb::Operation>(v->getValue());
    if ((pat == nullptr) || (pat->getOperands() == nullptr) || (pat->getOperands()->size() != 3)) return nullptr;
    return any_cast<hldb::TaggedPattern>(pat->getOperands()->at(i));
  }

  static void ExpectConstant(const hldb::Any *e, std::string_view text, std::string_view why) {
    const hldb::Constant *const c = any_cast<hldb::Constant>(e);
    ASSERT_NE(c, nullptr) << why << ": a literal";
    EXPECT_EQ(c->getDecompile(), text) << why;
  }
};

TEST_F(PatternOrderTest, AbcIsUnpackedStructOfABc1Bc2) {
  const hldb::Variable *const v = getAbc();
  ASSERT_NE(v, nullptr);
  const hldb::Struct *const st = getStruct(v->getTypespec());
  ASSERT_NE(st, nullptr) << "7.2: ABC is of a structure type";
  EXPECT_FALSE(st->getPacked()) << "7.2: no 'packed' keyword";
  ASSERT_NE(st->getMembers(), nullptr);
  ASSERT_EQ(st->getMembers()->size(), 3u);
  const char *const names[] = {"A", "BC1", "BC2"};
  for (size_t i = 0; i < 3; ++i) EXPECT_EQ(st->getMembers()->at(i)->getName(), names[i]) << "declaration order";
  for (size_t i = 1; i < 3; ++i) {
    const hldb::Struct *const inner = getStruct(st->getMembers()->at(i)->getTypespec());
    ASSERT_NE(inner, nullptr) << names[i] << " is a structure";
    ASSERT_NE(inner->getMembers(), nullptr);
    ASSERT_EQ(inner->getMembers()->size(), 2u);
    EXPECT_EQ(inner->getMembers()->at(0)->getName(), "B");
    EXPECT_EQ(inner->getMembers()->at(1)->getName(), "C");
  }
}

TEST_F(PatternOrderTest, InitializerIsThreeItemPattern) {
  const hldb::Variable *const v = getAbc();
  ASSERT_NE(v, nullptr);
  const hldb::Operation *const pat = any_cast<hldb::Operation>(v->getValue());
  ASSERT_NE(pat, nullptr) << "ABC has an initializer";
  EXPECT_EQ(pat->getOpType(), vpiAssignmentPatternOp) << "10.9.2: an assignment pattern";
  ASSERT_NE(pat->getOperands(), nullptr);
  ASSERT_EQ(pat->getOperands()->size(), 3u);
  const hldb::RefObj *const k0 = getItem(0)->getTag<hldb::RefObj>();
  const hldb::RefObj *const k2 = getItem(2)->getTag<hldb::RefObj>();
  ASSERT_NE(k0, nullptr);
  ASSERT_NE(k2, nullptr);
  EXPECT_EQ(k0->getName(), "BC1") << "source order";
  EXPECT_EQ(k2->getName(), "BC2") << "source order";
}

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG (structure
// pattern member keys not bound) in the file header.
TEST_F(PatternOrderTest, KeysBc1AndBc2BindToMembers) {
  const char *const names[] = {"BC1", "BC2"};
  const size_t items[] = {0, 2};
  for (size_t i = 0; i < 2; ++i) {
    const hldb::TaggedPattern *const tp = getItem(items[i]);
    ASSERT_NE(tp, nullptr);
    const hldb::RefObj *const key = tp->getTag<hldb::RefObj>();
    ASSERT_NE(key, nullptr);
    const hldb::TypespecMember *const tm = key->getActual<hldb::TypespecMember>();
    EXPECT_NE(tm, nullptr) << "10.9.2: \"A member:value specifies an explicit value for a named member of the "
                              "structure.\" -- "
                           << names[i];
    if (tm != nullptr) EXPECT_EQ(tm->getName(), names[i]);
  }
}

TEST_F(PatternOrderTest, IntIsATypeKey) {
  const hldb::TaggedPattern *const tp = getItem(1);
  ASSERT_NE(tp, nullptr);
  const hldb::RefTypespec *const key = tp->getTag<hldb::RefTypespec>();
  ASSERT_NE(key, nullptr) << "10.9.2: 'int:' is a type key";
  EXPECT_NE(key->getActual<hldb::IntTypespec>(), nullptr) << "the key names int";
  ExpectConstant(tp->getPattern(), "0", "the value of the int key");
}

TEST_F(PatternOrderTest, NestedPatternsHoldTheirItems) {
  const hldb::TaggedPattern *const bc1 = getItem(0);
  ASSERT_NE(bc1, nullptr);
  const hldb::Operation *const p1 = bc1->getPattern<hldb::Operation>();
  ASSERT_NE(p1, nullptr);
  EXPECT_EQ(p1->getOpType(), vpiAssignmentPatternOp);
  ASSERT_NE(p1->getOperands(), nullptr);
  ASSERT_EQ(p1->getOperands()->size(), 2u) << "10.9.2: a positional pattern of 2 items";
  ExpectConstant(p1->getOperands()->at(0), "1'b1", "B's value");
  ExpectConstant(p1->getOperands()->at(1), "1.0", "C's value");
  const hldb::TaggedPattern *const bc2 = getItem(2);
  ASSERT_NE(bc2, nullptr);
  const hldb::Operation *const p2 = bc2->getPattern<hldb::Operation>();
  ASSERT_NE(p2, nullptr);
  ASSERT_NE(p2->getOperands(), nullptr);
  ASSERT_EQ(p2->getOperands()->size(), 1u);
  const hldb::TaggedPattern *const def = any_cast<hldb::TaggedPattern>(p2->getOperands()->at(0));
  ASSERT_NE(def, nullptr);
  const hldb::RefObj *const dk = def->getTag<hldb::RefObj>();
  ASSERT_NE(dk, nullptr);
  EXPECT_EQ(dk->getName(), "default");
  ExpectConstant(def->getPattern(), "0", "the default value");
}

TEST_F(PatternOrderTest, NoFatalSyntaxOrErrorDiagnostics) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0) << "the file is legal SystemVerilog";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
