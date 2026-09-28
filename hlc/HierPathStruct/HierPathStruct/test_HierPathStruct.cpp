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

// Tests for tests/HierPathStruct/dut.sv:
//   package gpio_reg_pkg;
//     typedef struct packed { logic [31:0] d; logic de; } gpio_hw2reg_tt;
//     typedef struct packed { gpio_hw2reg_tt data_in; } gpio_hw2reg_t;
//   endpackage
//   module dut();
//     import gpio_reg_pkg::*;
//     gpio_hw2reg_t hw2reg;
//     assign hw2reg.data_in.de = 1'b1;
//   endmodule
//
// This exercises a hierarchical path (IEEE 1800-2023 Sec 23.6, hierarchical
// names) selecting into an unpacked-declared struct variable's nested packed
// struct members: hw2reg -> data_in -> de.
//
// `gpio_hw2reg_t hw2reg;` has no net-type keyword, so per IEEE 1800-2023
// Sec 6.7/6.8 it is a variable_declaration, not a net_declaration.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/cont_assign.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/ref_obj.h>
#include <hldb/typespec_member.h>
#include <hldb/variable.h>

namespace hlc {

class HierPathStructTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "HierPathStruct.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getDut() { return hldb::findByName<hldb::Module>("dut", m_design->getAllModules()); }

  static const hldb::ContAssign *getFirstContAssign() {
    const hldb::Module *const dut = getDut();
    if (dut == nullptr || dut->getContAssigns() == nullptr || dut->getContAssigns()->empty()) return nullptr;
    return dut->getContAssigns()->at(0);
  }
};

TEST_F(HierPathStructTest, ModuleDutExists) { EXPECT_NE(getDut(), nullptr); }

TEST_F(HierPathStructTest, VariableHw2regExists) {
  const hldb::Module *const dut = getDut();
  ASSERT_NE(dut, nullptr);
  ASSERT_NE(dut->getVariables(), nullptr);
  const hldb::Variable *const hw2reg = hldb::findByName<hldb::Variable>("hw2reg", dut->getVariables());
  ASSERT_NE(hw2reg, nullptr) << "variable 'hw2reg' not found";
}

// `hw2reg` has no net-type keyword and must not also appear as a Net.
TEST_F(HierPathStructTest, VariableHw2regIsNotDuplicatedAsNet) {
  const hldb::Module *const dut = getDut();
  ASSERT_NE(dut, nullptr);
  if (dut->getNets() != nullptr) {
    EXPECT_EQ(hldb::findByName<hldb::Net>("hw2reg", dut->getNets()), nullptr);
  }
}

TEST_F(HierPathStructTest, OneContAssignExists) {
  const hldb::Module *const dut = getDut();
  ASSERT_NE(dut, nullptr);
  ASSERT_NE(dut->getContAssigns(), nullptr);
  EXPECT_EQ(dut->getContAssigns()->size(), 1u);
}

TEST_F(HierPathStructTest, ContAssignLhsIsHierPathWithThreeElems) {
  const hldb::ContAssign *const ca = getFirstContAssign();
  ASSERT_NE(ca, nullptr);

  const hldb::RefObj *const lhs = ca->getLhs<hldb::RefObj>();
  ASSERT_NE(ca->getLhs(), nullptr);
  ASSERT_NE(lhs, nullptr) << "lhs is not a RefObj";
  EXPECT_EQ(lhs->getName(), std::string_view("hw2reg.data_in.de"));

  ASSERT_NE(lhs->getPathElems(), nullptr);
  ASSERT_EQ(lhs->getPathElems()->size(), 3u) << "expected hw2reg -> data_in -> de";
}

TEST_F(HierPathStructTest, FirstPathElemResolvesToVariableHw2reg) {
  const hldb::ContAssign *const ca = getFirstContAssign();
  ASSERT_NE(ca, nullptr);
  const hldb::RefObj *const lhs = ca->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  ASSERT_NE(lhs->getPathElems(), nullptr);
  ASSERT_EQ(lhs->getPathElems()->size(), 3u);

  const hldb::RefObj *const elem0 = any_cast<hldb::RefObj>(lhs->getPathElems()->at(0));
  ASSERT_NE(elem0, nullptr);
  EXPECT_EQ(elem0->getName(), std::string_view("hw2reg"));
  ASSERT_NE(elem0->getActual(), nullptr);
  EXPECT_NE(elem0->getActual<hldb::Variable>(), nullptr) << "'hw2reg' should resolve to the Variable declaration";
}

TEST_F(HierPathStructTest, SecondPathElemResolvesToDataInMember) {
  const hldb::ContAssign *const ca = getFirstContAssign();
  ASSERT_NE(ca, nullptr);
  const hldb::RefObj *const lhs = ca->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  ASSERT_NE(lhs->getPathElems(), nullptr);
  ASSERT_EQ(lhs->getPathElems()->size(), 3u);

  const hldb::RefObj *const elem1 = any_cast<hldb::RefObj>(lhs->getPathElems()->at(1));
  ASSERT_NE(elem1, nullptr);
  EXPECT_EQ(elem1->getName(), std::string_view("data_in"));
  ASSERT_NE(elem1->getActual(), nullptr);
  EXPECT_NE(elem1->getActual<hldb::TypespecMember>(), nullptr)
      << "'data_in' should resolve to the struct member declaration";
}

TEST_F(HierPathStructTest, ThirdPathElemResolvesToDeMember) {
  const hldb::ContAssign *const ca = getFirstContAssign();
  ASSERT_NE(ca, nullptr);
  const hldb::RefObj *const lhs = ca->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  ASSERT_NE(lhs->getPathElems(), nullptr);
  ASSERT_EQ(lhs->getPathElems()->size(), 3u);

  const hldb::RefObj *const elem2 = any_cast<hldb::RefObj>(lhs->getPathElems()->at(2));
  ASSERT_NE(elem2, nullptr);
  EXPECT_EQ(elem2->getName(), std::string_view("de"));
  ASSERT_NE(elem2->getActual(), nullptr);
  EXPECT_NE(elem2->getActual<hldb::TypespecMember>(), nullptr)
      << "'de' should resolve to the innermost struct member declaration";
}

TEST_F(HierPathStructTest, ContAssignRhsIsOneBitConstant) {
  const hldb::ContAssign *const ca = getFirstContAssign();
  ASSERT_NE(ca, nullptr);
  ASSERT_NE(ca->getRhs(), nullptr);
  const hldb::Constant *const rhs = ca->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), std::string_view("1'b1"));
}

TEST_F(HierPathStructTest, CompilerReportsZeroErrors) {
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
