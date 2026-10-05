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

// Tests for dut.sv (tags: MultiSelect)
//   module top;
//     logic a [1:0][1:0];
//     logic b [1:0][1:0];
//
//     assign a[0][0] = 1'b0;
//     assign a[0][1] = 1'b1;
//     assign a[1][0] = 1'b1;
//     assign a[1][1] = 1'b1;
//
//     assign b = a;
//   endmodule
//
// What is checked (IEEE 1800-2023):
//   - 6.8 / 7.4.2: 'a' and 'b' are variables (no net-type keyword) declared
//     as two-dimensional unpacked arrays of 1-bit 'logic'. The array type is
//     a static (fixed-size, 7.4.2) unpacked array whose element type is
//     itself an unpacked array [1:0] of logic. 7.4.5: "the leftmost dimension
//     varies most slowly", so the outer ArrayTypespec carries the first
//     ('[1:0]' at column 10) dimension and its element carries the second
//     (column 15).
//   - 10.3.2: module 'top' has five continuous assignments.
//   - 7.4.5 / 11.5.2: 'a[i][j]' selects a single element: the LHS is a
//     select with constant index j whose prefix is a select with constant
//     index i on 'a'; the base name binds to variable 'a'.
//   - 5.7.1: '1'b0' / '1'b1' are sized binary literals of size 1 with the
//     expected values.
//   - 7.6: 'assign b = a;' is a whole-unpacked-array assignment between
//     arrays of identical shape; both sides are simple references binding
//     to variables 'b' and 'a'.
//   - 6.5: each element of 'a' is written by exactly one continuous
//     assignment (independent elements of an array), so no multiple-driver
//     diagnostic for 'a'.
//
// What is NOT checked and why:
//   - simulation values of 'b'. This flow does not elaborate/evaluate.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/bit_select.h>
#include <hldb/constant.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <string>

namespace hlc {

class MultiSelectTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "MultiSelect.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::Variable *getVar(std::string_view name) {
    const hldb::Module *const top = getTop();
    return (top == nullptr) ? nullptr : hldb::findByName<hldb::Variable>(name, top->getVariables());
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

  static void checkTwoDimUnpackedLogic(std::string_view name) {
    const hldb::Variable *const v = getVar(name);
    ASSERT_NE(v, nullptr) << "6.8: '" << name << "' must be a variable";
    ASSERT_NE(v->getTypespec(), nullptr);
    const hldb::ArrayTypespec *const outer = any_cast<hldb::ArrayTypespec>(v->getTypespec()->getActual());
    ASSERT_NE(outer, nullptr) << "7.4.2: '" << name << "' is an unpacked array";
    EXPECT_FALSE(outer->getPacked());
    EXPECT_EQ(outer->getArrayType(), vpiStaticArray);
    ASSERT_NE(outer->getRange(), nullptr);
    EXPECT_EQ(constValue(outer->getRange()->getLeftExpr()), 1);
    EXPECT_EQ(constValue(outer->getRange()->getRightExpr()), 0);
    EXPECT_EQ(outer->getRange()->getStartColumn(), 10u) << "outer dimension is the leftmost '[1:0]' (7.4.5)";

    ASSERT_NE(outer->getElemTypespec(), nullptr);
    const hldb::ArrayTypespec *const inner = any_cast<hldb::ArrayTypespec>(outer->getElemTypespec()->getActual());
    ASSERT_NE(inner, nullptr) << "second unpacked dimension";
    EXPECT_FALSE(inner->getPacked());
    EXPECT_EQ(inner->getArrayType(), vpiStaticArray);
    ASSERT_NE(inner->getRange(), nullptr);
    EXPECT_EQ(constValue(inner->getRange()->getLeftExpr()), 1);
    EXPECT_EQ(constValue(inner->getRange()->getRightExpr()), 0);
    EXPECT_EQ(inner->getRange()->getStartColumn(), 15u);

    ASSERT_NE(inner->getElemTypespec(), nullptr);
    const hldb::LogicTypespec *const elem = any_cast<hldb::LogicTypespec>(inner->getElemTypespec()->getActual());
    ASSERT_NE(elem, nullptr) << "element type is 'logic'";
    EXPECT_TRUE(elem->getRanges() == nullptr || elem->getRanges()->empty()) << "1-bit scalar logic element";
  }

  static void checkElementAssign(size_t index, int64_t i, int64_t j, std::string_view literal) {
    const hldb::ContAssign *const ca = getAssign(index);
    ASSERT_NE(ca, nullptr);
    const hldb::BitSelect *const outer = any_cast<hldb::BitSelect>(ca->getLhs());
    ASSERT_NE(outer, nullptr) << "a[" << i << "][" << j << "] is a single-element select (7.4.5)";
    EXPECT_EQ(constValue(outer->getIndex()), j);
    const hldb::BitSelect *const inner = any_cast<hldb::BitSelect>(outer->getPrefix());
    ASSERT_NE(inner, nullptr);
    EXPECT_EQ(constValue(inner->getIndex()), i);
    const hldb::RefObj *const base = any_cast<hldb::RefObj>(inner->getPrefix());
    ASSERT_NE(base, nullptr);
    EXPECT_EQ(base->getName(), "a");
    ASSERT_NE(base->getActual(), nullptr);
    EXPECT_EQ(base->getActual(), getVar("a"));

    const hldb::Constant *const rhs = any_cast<hldb::Constant>(ca->getRhs());
    ASSERT_NE(rhs, nullptr);
    EXPECT_EQ(rhs->getDecompile(), literal);
    EXPECT_EQ(rhs->getConstType(), vpiBinaryConst);
    EXPECT_EQ(rhs->getSize(), 1) << "5.7.1: sized literal of width 1";
  }
};

TEST_F(MultiSelectTest, ModuleTopExists) { EXPECT_NE(getTop(), nullptr); }

// 6.8 / 7.4.2
TEST_F(MultiSelectTest, AIsTwoDimUnpackedLogicArray) { checkTwoDimUnpackedLogic("a"); }

TEST_F(MultiSelectTest, BIsTwoDimUnpackedLogicArray) { checkTwoDimUnpackedLogic("b"); }

// 10.3.2
TEST_F(MultiSelectTest, TopHasFiveContAssigns) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  EXPECT_EQ(top->getContAssigns()->size(), 5u);
}

// 7.4.5 / 11.5.2 / 5.7.1
TEST_F(MultiSelectTest, AssignA00) { checkElementAssign(0, 0, 0, "1'b0"); }

TEST_F(MultiSelectTest, AssignA01) { checkElementAssign(1, 0, 1, "1'b1"); }

TEST_F(MultiSelectTest, AssignA10) { checkElementAssign(2, 1, 0, "1'b1"); }

TEST_F(MultiSelectTest, AssignA11) { checkElementAssign(3, 1, 1, "1'b1"); }

// 7.6: whole unpacked array assignment
TEST_F(MultiSelectTest, WholeArrayAssignBEqualsA) {
  const hldb::ContAssign *const ca = getAssign(4);
  ASSERT_NE(ca, nullptr);
  const hldb::RefObj *const lhs = any_cast<hldb::RefObj>(ca->getLhs());
  const hldb::RefObj *const rhs = any_cast<hldb::RefObj>(ca->getRhs());
  ASSERT_NE(lhs, nullptr) << "'b' is a plain reference to the whole array";
  ASSERT_NE(rhs, nullptr) << "'a' is a plain reference to the whole array";
  EXPECT_EQ(lhs->getName(), "b");
  EXPECT_EQ(rhs->getName(), "a");
  ASSERT_NE(lhs->getActual(), nullptr);
  ASSERT_NE(rhs->getActual(), nullptr);
  EXPECT_EQ(lhs->getActual(), getVar("b"));
  EXPECT_EQ(rhs->getActual(), getVar("a"));
}

// Name binding / 6.5
TEST_F(MultiSelectTest, NoBindingOrMultiDriverErrors) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "a"), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "b"), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::HLDB_MULTIPLE_CONT_ASSIGN), nullptr)
      << "6.5: each array element of 'a' is covered by a single continuous assignment";
  EXPECT_EQ(findError(ErrorDefinition::COMP_MULTIPLE_ASSIGNING_PROCESSES), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
