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

// Tests for tests/HierPathVarArray/dut.sv:
//   package pkg;
//     typedef struct packed { logic x; } a;
//     typedef a b[5:0];
//   endpackage
//   module top(output o);
//     pkg::b c;
//     assign c[1].x = 1;
//     assign o = c[1].x;
//   endmodule
//
// Unlike HierPathTypespec's 'typedef a[5:0] b' (dimension before the name,
// a packed array), here 'typedef a b[5:0]' places the dimension *after* the
// name 'b', which per IEEE 1800-2023 Sec 6.20 / Sec 7.4.2 makes 'b' an
// unpacked (variable) array of struct 'a'. This exercises a hierarchical
// path into that variable array: c[1] selects an 'a'-typed unpacked-array
// element, and '.x' then selects its packed member.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/bit_select.h>
#include <hldb/cont_assign.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/struct.h>
#include <hldb/struct_typespec.h>
#include <hldb/typedef.h>
#include <hldb/typedef_typespec.h>
#include <hldb/typespec_member.h>
#include <hldb/variable.h>

namespace hlc {

class HierPathVarArrayTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "HierPathVarArray.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::ArrayTypespec *getVariableCArrayTypespec() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getVariables() == nullptr) return nullptr;
    const hldb::Variable *const c = hldb::findByName<hldb::Variable>("c", top->getVariables());
    if (c == nullptr || c->getTypespec() == nullptr) return nullptr;
    return c->getTypespec<hldb::ArrayTypespec>();
  }

  static const hldb::ContAssign *getContAssign(size_t n) {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getContAssigns() == nullptr || top->getContAssigns()->size() <= n) return nullptr;
    return top->getContAssigns()->at(n);
  }
};

TEST_F(HierPathVarArrayTest, ModuleTopExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(HierPathVarArrayTest, VariableCResolvesToUnpackedArrayOfStruct) {
  const hldb::ArrayTypespec *const at = getVariableCArrayTypespec();
  ASSERT_NE(at, nullptr) << "'c' should resolve (via TypedefTypespec 'b') to an ArrayTypespec";
  EXPECT_FALSE(at->getPacked()) << "'typedef a b[5:0]' places the dimension after the name, "
                                   "so 'b' must be an unpacked (variable) array type";
}

TEST_F(HierPathVarArrayTest, ArrayElemTypespecResolvesToStructAWithMemberX) {
  const hldb::ArrayTypespec *const at = getVariableCArrayTypespec();
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getElemTypespec(), nullptr);
  const hldb::StructTypespec *const elemSt = at->getElemTypespec()->getActual<hldb::StructTypespec>();
  ASSERT_NE(elemSt, nullptr) << "array element should resolve to struct 'a'";
  const hldb::Struct *const s = elemSt->getStruct();
  ASSERT_NE(s, nullptr);
  EXPECT_TRUE(s->getPacked()) << "struct 'a' itself is still packed";
  ASSERT_NE(s->getMembers(), nullptr);
  ASSERT_EQ(s->getMembers()->size(), 1u);
  EXPECT_EQ(s->getMembers()->at(0)->getName(), std::string_view("x"));
}

TEST_F(HierPathVarArrayTest, TwoContAssignsExist) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  EXPECT_EQ(top->getContAssigns()->size(), 2u);
}

// assign c[1].x = 1;
TEST_F(HierPathVarArrayTest, FirstContAssignLhsIsIndexThenMemberHierPath) {
  const hldb::ContAssign *const ca = getContAssign(0);
  ASSERT_NE(ca, nullptr);
  ASSERT_NE(ca->getLhs(), nullptr);
  const hldb::RefObj *const lhs = ca->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), std::string_view("c[1].x"));
  ASSERT_NE(lhs->getPathElems(), nullptr);
  ASSERT_EQ(lhs->getPathElems()->size(), 2u) << "expected c[1] -> x";

  const hldb::BitSelect *const idxElem = any_cast<hldb::BitSelect>(lhs->getPathElems()->at(0));
  ASSERT_NE(idxElem, nullptr) << "first path elem should be the bit-select 'c[1]'";
  ASSERT_NE(idxElem->getPrefix(), nullptr);
  const hldb::RefObj *const prefix = idxElem->getPrefix<hldb::RefObj>();
  ASSERT_NE(prefix, nullptr);
  EXPECT_EQ(prefix->getName(), std::string_view("c"));
  ASSERT_NE(prefix->getActual(), nullptr);
  EXPECT_NE(prefix->getActual<hldb::Variable>(), nullptr);

  ASSERT_NE(idxElem->getIndex(), nullptr);
  const hldb::Constant *const idx = idxElem->getIndex<hldb::Constant>();
  ASSERT_NE(idx, nullptr);
  EXPECT_EQ(idx->getDecompile(), std::string_view("1"));

  const hldb::RefObj *const memberElem = any_cast<hldb::RefObj>(lhs->getPathElems()->at(1));
  ASSERT_NE(memberElem, nullptr);
  EXPECT_EQ(memberElem->getName(), std::string_view("x"));
  ASSERT_NE(memberElem->getActual(), nullptr);
  EXPECT_NE(memberElem->getActual<hldb::TypespecMember>(), nullptr)
      << "'x' should resolve to struct 'a's member declaration";
}

TEST_F(HierPathVarArrayTest, FirstContAssignRhsIsConstantOne) {
  const hldb::ContAssign *const ca = getContAssign(0);
  ASSERT_NE(ca, nullptr);
  ASSERT_NE(ca->getRhs(), nullptr);
  const hldb::Constant *const rhs = ca->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), std::string_view("1"));
}

// assign o = c[1].x;
TEST_F(HierPathVarArrayTest, SecondContAssignRhsIsSameHierPathShape) {
  const hldb::ContAssign *const ca = getContAssign(1);
  ASSERT_NE(ca, nullptr);
  ASSERT_NE(ca->getRhs(), nullptr);
  const hldb::RefObj *const rhs = ca->getRhs<hldb::RefObj>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getName(), std::string_view("c[1].x"));
  ASSERT_NE(rhs->getPathElems(), nullptr);
  EXPECT_EQ(rhs->getPathElems()->size(), 2u);
}

TEST_F(HierPathVarArrayTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
