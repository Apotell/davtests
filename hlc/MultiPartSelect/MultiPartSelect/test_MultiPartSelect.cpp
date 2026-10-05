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

// Tests for dut.sv (tags: MultiPartSelect)
//   typedef struct packed {
//       logic [1:0][3:0] a;
//   } tier1;
//
//   module top();
//       tier1 xx;
//       assign xx.a[1][3:2] = 2;
//       assign xx.a[1][1:0] = 3;
//   endmodule
//
// What is checked (IEEE 1800-2023):
//   - 7.2.1: 'tier1' is a $unit-level typedef of a packed struct with a
//     single member 'a' whose type is 'logic [1:0][3:0]' -- a multi-
//     dimensional packed array (7.4.1), i.e. two packed ranges [1:0] and
//     [3:0], in that order.
//   - 6.8: 'tier1 xx;' has no net-type keyword, so 'xx' is a variable of
//     type 'tier1' declared in module 'top'.
//   - 10.3.2: module 'top' has exactly two continuous assignments.
//   - 7.2 / 7.4.5 / 11.5.1: each LHS is the member select 'xx.a' followed by
//     an element select '[1]' (single element of the outer packed
//     dimension) and then a part-select '[3:2]' / '[1:0]' of the inner
//     dimension. The LHS is modeled as a RefObj path ('xx' then the
//     select on 'a'); the outer-most select is a PartSelect whose prefix is a
//     BitSelect with index 1 on member 'a'.
//   - name binding: 'xx' binds to the variable and 'a' binds to the struct
//     member (no COMP_FAILED_TO_BIND).
//   - 6.5: the two assignments write disjoint bits of a packed aggregate
//     ("Each bit in a packed type is also an independent element"), so no
//     multiple-continuous-assignment diagnostic must be raised for 'xx'.
//   - RHS literals are 2 and 3 respectively.
//
// What is NOT checked and why:
//   - the resulting bit values of xx (simulation semantics; this flow does
//     not elaborate or evaluate).
//   - the self-determined size / signedness of the unsized literals (5.7.1)
//     beyond their value: HLDB's Constant::getSize() encoding for unsized
//     literals is a tool convention.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/bit_select.h>
#include <hldb/constant.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/part_select.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/struct.h>
#include <hldb/struct_typespec.h>
#include <hldb/typedef.h>
#include <hldb/typedef_typespec.h>
#include <hldb/typespec_member.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <string>

namespace hlc {

class MultiPartSelectTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "MultiPartSelect.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::Struct *getTier1Struct() {
    const hldb::Typedef *const td = hldb::findByName<hldb::Typedef>("tier1", m_design->getTypedefs());
    if (td == nullptr || td->getAlias() == nullptr) return nullptr;
    const hldb::StructTypespec *const sts = any_cast<hldb::StructTypespec>(td->getAlias()->getActual());
    return (sts == nullptr) ? nullptr : sts->getStruct();
  }

  static const hldb::ContAssign *getAssign(size_t index) {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getContAssigns() == nullptr) return nullptr;
    if (top->getContAssigns()->size() <= index) return nullptr;
    return top->getContAssigns()->at(index);
  }

  static int64_t constValue(const hldb::Any *any) {
    const hldb::Constant *const c = any_cast<hldb::Constant>(any);
    if (c == nullptr) return -1;
    return std::stoll(std::string(c->getDecompile()));
  }

  // Checks LHS shape 'xx.a[1][left:right]'.
  static void checkLhs(const hldb::ContAssign *ca, int64_t left, int64_t right) {
    ASSERT_NE(ca, nullptr);
    const hldb::RefObj *const lhs = any_cast<hldb::RefObj>(ca->getLhs());
    ASSERT_NE(lhs, nullptr) << "LHS 'xx.a[...]' should be a RefObj path";
    ASSERT_NE(lhs->getPathElems(), nullptr);
    ASSERT_EQ(lhs->getPathElems()->size(), 2u) << "path: 'xx' then select on member 'a'";

    const hldb::RefObj *const head = any_cast<hldb::RefObj>(lhs->getPathElems()->at(0));
    ASSERT_NE(head, nullptr);
    EXPECT_EQ(head->getName(), "xx");
    ASSERT_NE(head->getActual(), nullptr) << "'xx' must bind";
    EXPECT_EQ(head->getActual()->getAnyType(), hldb::AnyType::Variable);

    const hldb::PartSelect *const ps = any_cast<hldb::PartSelect>(lhs->getPathElems()->at(1));
    ASSERT_NE(ps, nullptr) << "outer-most select '[" << left << ":" << right << "]' is a part-select (11.5.1)";
    ASSERT_NE(ps->getRange(), nullptr);
    EXPECT_EQ(constValue(ps->getRange()->getLeftExpr()), left);
    EXPECT_EQ(constValue(ps->getRange()->getRightExpr()), right);

    const hldb::BitSelect *const bs = any_cast<hldb::BitSelect>(ps->getPrefix());
    ASSERT_NE(bs, nullptr) << "'a[1]' selects one element of the outer packed dimension (7.4.5)";
    EXPECT_EQ(constValue(bs->getIndex()), 1);

    const hldb::RefObj *const member = any_cast<hldb::RefObj>(bs->getPrefix());
    ASSERT_NE(member, nullptr);
    EXPECT_EQ(member->getName(), "a");
    ASSERT_NE(member->getActual(), nullptr) << "member 'a' must bind to the struct member";
    EXPECT_EQ(member->getActual()->getAnyType(), hldb::AnyType::TypespecMember);
    const hldb::TypespecMember *const tm = any_cast<hldb::TypespecMember>(member->getActual());
    ASSERT_NE(tm, nullptr);
    EXPECT_EQ(tm->getName(), "a");
  }
};

// ===========================================================================
// 7.2.1: typedef struct packed { logic [1:0][3:0] a; } tier1;
// ===========================================================================

TEST_F(MultiPartSelectTest, Tier1IsPackedStructWithOneMember) {
  const hldb::Struct *const st = getTier1Struct();
  ASSERT_NE(st, nullptr) << "typedef 'tier1' must alias a struct type";
  EXPECT_TRUE(st->getPacked()) << "7.2.1: 'struct packed'";
  ASSERT_NE(st->getMembers(), nullptr);
  ASSERT_EQ(st->getMembers()->size(), 1u);
  EXPECT_EQ(st->getMembers()->at(0)->getName(), "a");
}

TEST_F(MultiPartSelectTest, MemberAIsTwoDimPackedLogic) {
  const hldb::Struct *const st = getTier1Struct();
  ASSERT_NE(st, nullptr);
  ASSERT_NE(st->getMembers(), nullptr);
  ASSERT_EQ(st->getMembers()->size(), 1u);
  const hldb::TypespecMember *const a = st->getMembers()->at(0);
  ASSERT_NE(a->getTypespec(), nullptr);
  const hldb::LogicTypespec *const lt = any_cast<hldb::LogicTypespec>(a->getTypespec()->getActual());
  ASSERT_NE(lt, nullptr) << "'a' is of type logic [1:0][3:0]";
  ASSERT_NE(lt->getRanges(), nullptr);
  ASSERT_EQ(lt->getRanges()->size(), 2u) << "7.4.1: two packed dimensions";
  EXPECT_EQ(constValue(lt->getRanges()->at(0)->getLeftExpr()), 1);
  EXPECT_EQ(constValue(lt->getRanges()->at(0)->getRightExpr()), 0);
  EXPECT_EQ(constValue(lt->getRanges()->at(1)->getLeftExpr()), 3);
  EXPECT_EQ(constValue(lt->getRanges()->at(1)->getRightExpr()), 0);
}

// ===========================================================================
// 6.8: 'tier1 xx;' declares a variable
// ===========================================================================

TEST_F(MultiPartSelectTest, XxIsVariableOfTypeTier1) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const xx = hldb::findByName<hldb::Variable>("xx", top->getVariables());
  ASSERT_NE(xx, nullptr) << "6.8: no net-type keyword, so 'xx' is a variable";
  ASSERT_NE(xx->getTypespec(), nullptr);
  const hldb::TypedefTypespec *const tts = any_cast<hldb::TypedefTypespec>(xx->getTypespec()->getActual());
  ASSERT_NE(tts, nullptr) << "'xx' must be typed by the typedef 'tier1'";
  EXPECT_EQ(tts->getName(), "tier1");
}

// ===========================================================================
// 10.3.2: two continuous assignments
// ===========================================================================

TEST_F(MultiPartSelectTest, TopHasTwoContAssigns) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  EXPECT_EQ(top->getContAssigns()->size(), 2u);
}

// ===========================================================================
// 7.4.5 / 11.5.1: xx.a[1][3:2] and xx.a[1][1:0]
// ===========================================================================

TEST_F(MultiPartSelectTest, FirstLhsIsXxA1PartSelect3To2) { checkLhs(getAssign(0), 3, 2); }

TEST_F(MultiPartSelectTest, SecondLhsIsXxA1PartSelect1To0) { checkLhs(getAssign(1), 1, 0); }

TEST_F(MultiPartSelectTest, RhsValuesAre2And3) {
  const hldb::ContAssign *const ca0 = getAssign(0);
  const hldb::ContAssign *const ca1 = getAssign(1);
  ASSERT_NE(ca0, nullptr);
  ASSERT_NE(ca1, nullptr);
  ASSERT_NE(any_cast<hldb::Constant>(ca0->getRhs()), nullptr);
  ASSERT_NE(any_cast<hldb::Constant>(ca1->getRhs()), nullptr);
  EXPECT_EQ(constValue(ca0->getRhs()), 2);
  EXPECT_EQ(constValue(ca1->getRhs()), 3);
}

// ===========================================================================
// Name binding and 6.5 (disjoint bits may each have one continuous assign)
// ===========================================================================

TEST_F(MultiPartSelectTest, NoBindingErrors) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "xx"), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "a"), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::COMP_UNDEFINED_TYPE, "tier1"), nullptr);
}

TEST_F(MultiPartSelectTest, DisjointBitAssignmentsAreLegal) {
  EXPECT_EQ(findError(ErrorDefinition::HLDB_MULTIPLE_CONT_ASSIGN), nullptr)
      << "6.5: each bit of a packed aggregate is an independent element; [3:2] and [1:0] do not overlap";
  EXPECT_EQ(findError(ErrorDefinition::COMP_MULTIPLE_ASSIGNING_PROCESSES), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
