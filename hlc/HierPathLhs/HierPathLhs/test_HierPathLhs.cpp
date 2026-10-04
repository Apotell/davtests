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

// Tests for HierPathLhs/dut.sv (tags: HierPathLhs)
//   module alert_handler_reg_wrap;
//     for (genvar k = 0; k < 1; k++) begin : gen_alert_cause
//       assign hw2reg.alert_cause[k].d  = 1'b1;
//     end
//   endmodule
//
//   module top;
//     typedef struct packed { int x; } struct_t;
//     struct_t [1:0][2:0] a;
//     assign a[0][0].x[0] = 1;
//   endmodule
//
// Checked (per IEEE 1800-2023 Sec 23.6 -- Hierarchical names, Sec 11.5.1 --
// Vector bit-select and part-select addressing):
//   - "a[0][0].x[0]" is a hierarchical-path-shaped reference (indexed
//     unpacked-array selects mixed with a packed-struct member dot, per the
//     `hierarchical_identifier` grammar) used as the LHS of a continuous
//     assignment. It resolves as a RefObj with getPathElems() == two
//     segments, "a[0][0]" and "x[0]" (one per dot-separated component,
//     each carrying its own indices), and getActual() resolving to the
//     struct member "x"
//   - "hw2reg" in "alert_handler_reg_wrap" is never declared anywhere in
//     the design, so "hw2reg.alert_cause[k].d" must fail to bind (Sec 6.3
//     -- an identifier must be declared before use) rather than silently
//     being treated as an implicit net or accepted
//   - "gen_alert_cause" (an unconditional-count "for (genvar k=0;k<1;k++)"
//     generate-for loop) is modeled as a GenScopeArray of size 1

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/gen_scope_array.h>
#include <hldb/module.h>
#include <hldb/ref_obj.h>
#include <hldb/typespec_member.h>
#include <hldb/vpi_user.h>

namespace hlc {

class HierPathLhsTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "HierPathLhs.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getModule(std::string_view name) {
    return hldb::findByName<hldb::Module>(name, m_design->getAllModules());
  }
};

TEST_F(HierPathLhsTest, ModulesExist) {
  EXPECT_NE(getModule("alert_handler_reg_wrap"), nullptr);
  EXPECT_NE(getModule("top"), nullptr);
}

TEST_F(HierPathLhsTest, GenAlertCauseIsAGenScopeArrayOfSizeOne) {
  const hldb::Module *const wrap = getModule("alert_handler_reg_wrap");
  ASSERT_NE(wrap, nullptr);
  ASSERT_NE(wrap->getGenScopeArrays(), nullptr);
  const hldb::GenScopeArray *const gen =
      hldb::findByName<hldb::GenScopeArray>("gen_alert_cause", wrap->getGenScopeArrays());
  ASSERT_NE(gen, nullptr) << "'gen_alert_cause' generate-for block not found";
  EXPECT_EQ(gen->getSize(), 1);
  ASSERT_NE(gen->getGenScopes(), nullptr);
  EXPECT_EQ(gen->getGenScopes()->size(), 1u);
}

TEST_F(HierPathLhsTest, UndeclaredHw2regFailsToBind) {
  EXPECT_NE(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "hw2reg"), nullptr)
      << "'hw2reg' is never declared and must fail to bind";
}

TEST_F(HierPathLhsTest, StructMemberBitSelectHierPathLhsResolvesToMemberX) {
  const hldb::Module *const top = getModule("top");
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  ASSERT_EQ(top->getContAssigns()->size(), 1u);
  const hldb::ContAssign *const ca = top->getContAssigns()->at(0);
  ASSERT_NE(ca, nullptr);

  const hldb::RefObj *const lhs = ca->getLhs<hldb::RefObj>();
  ASSERT_NE(ca->getLhs(), nullptr);
  ASSERT_NE(lhs, nullptr) << "lhs of 'a[0][0].x[0] = 1' is not a RefObj hierarchical path";

  ASSERT_NE(lhs->getPathElems(), nullptr);
  ASSERT_EQ(lhs->getPathElems()->size(), 2u) << "one path element per dot-separated segment: 'a[0][0]' and 'x[0]'";
  EXPECT_EQ(lhs->getPathElems()->at(0)->getName(), std::string_view{"a"});
  EXPECT_EQ(lhs->getPathElems()->at(1)->getName(), std::string_view{"x"});

  ASSERT_NE(lhs->getActual(), nullptr);
  const hldb::TypespecMember *const member = lhs->getActual<hldb::TypespecMember>();
  ASSERT_NE(member, nullptr) << "'a[0][0].x[0]' should resolve to the struct member 'x'";
  EXPECT_EQ(member->getName(), std::string_view{"x"});
}

TEST_F(HierPathLhsTest, TopModuleReportsNoBindingErrors) {
  // "top" is a self-contained module (unlike "alert_handler_reg_wrap") --
  // its single continuous assignment must bind cleanly.
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "a"), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "x"), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
