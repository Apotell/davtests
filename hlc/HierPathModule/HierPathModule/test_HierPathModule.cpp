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

// Tests for tests/HierPathModule/dut.sv:
//
//   module bottom();
//     assign o = medium.c;
//   endmodule
//
//   module medium();
//     wire c;
//     bottom b1();
//   endmodule
//
//   module top();
//      medium u1();
//   endmodule
//
// "medium.c" written from inside "bottom" is a hierarchical-name reference
// that uses the enclosing module's *type* name ("medium") rather than an
// instance name. IEEE 1800-2023 Sec 23.8 (Upwards name referencing) allows
// exactly this: "The name of a module or module instance is sufficient to
// identify the module and its location in the hierarchy ... Variables can be
// referenced if the name of the higher level module or its instance name is
// known", with Syntax 23-8 'upward_name_reference ::= module_identifier.
// item_name' (item_name includes net_identifier). "bottom" is instantiated
// inside "medium" (as b1), so "medium.c" reaches up to medium's wire "c".
//
// Checked:
//   - module "bottom" has exactly one ContAssign
//   - the RHS is parsed as a hierarchical RefObj "medium.c" with 2 path
//     elements ("medium", "c") -- this is guaranteed by the grammar
//     regardless of whether the name ultimately binds
//   - per Sec 23.8 the upward reference resolves: path element "medium" binds
//     to module "medium", and the RefObj's getActual() is medium's Net "c"

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/ref_obj.h>

#include <gtest/gtest.h>

#include <string_view>

namespace hlc {

class HierPathModuleTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "HierPathModule.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getBottom() {
    return hldb::findByDefName<hldb::Module>("bottom", m_design->getAllModules());
  }
};

TEST_F(HierPathModuleTest, ModulesExist) {
  EXPECT_NE(getBottom(), nullptr);
  EXPECT_NE(hldb::findByDefName<hldb::Module>("medium", m_design->getAllModules()), nullptr);
  EXPECT_NE(hldb::findByDefName<hldb::Module>("top", m_design->getAllModules()), nullptr);
}

TEST_F(HierPathModuleTest, BottomHasOneContAssign) {
  const hldb::Module *const bottom = getBottom();
  ASSERT_NE(bottom, nullptr);
  ASSERT_NE(bottom->getContAssigns(), nullptr);
  EXPECT_EQ(bottom->getContAssigns()->size(), 1u);
}

TEST_F(HierPathModuleTest, RhsIsHierarchicalRefObjWithTwoPathElems) {
  const hldb::Module *const bottom = getBottom();
  ASSERT_NE(bottom, nullptr);
  ASSERT_NE(bottom->getContAssigns(), nullptr);
  ASSERT_EQ(bottom->getContAssigns()->size(), 1u);
  const hldb::ContAssign *const ca = bottom->getContAssigns()->at(0);
  ASSERT_NE(ca, nullptr);

  ASSERT_NE(ca->getRhs(), nullptr);
  const hldb::RefObj *const rhs = ca->getRhs<hldb::RefObj>();
  ASSERT_NE(rhs, nullptr) << "'medium.c' RHS should be a hierarchical RefObj";
  EXPECT_EQ(rhs->getName(), std::string_view{"medium.c"});

  ASSERT_NE(rhs->getPathElems(), nullptr);
  ASSERT_EQ(rhs->getPathElems()->size(), 2u);
  EXPECT_EQ(rhs->getPathElems()->at(0)->getName(), std::string_view{"medium"});
  EXPECT_EQ(rhs->getPathElems()->at(1)->getName(), std::string_view{"c"});
}

TEST_F(HierPathModuleTest, HierarchicalReferenceByModuleTypeNameResolvesUpward) {
  const hldb::Module *const bottom = getBottom();
  ASSERT_NE(bottom, nullptr);
  ASSERT_NE(bottom->getContAssigns(), nullptr);
  ASSERT_EQ(bottom->getContAssigns()->size(), 1u);
  const hldb::RefObj *const rhs = bottom->getContAssigns()->at(0)->getRhs<hldb::RefObj>();
  ASSERT_NE(rhs, nullptr);
  // EXPECT_EQ(rhs->getActual(), nullptr)
  //     << "'medium.c' names a module type, not an instance -- per Sec 23.8 this cannot bind";
  ASSERT_NE(rhs->getPathElems(), nullptr);
  ASSERT_EQ(rhs->getPathElems()->size(), 2u);
  const hldb::RefObj *const scope = any_cast<hldb::RefObj>(rhs->getPathElems()->at(0));
  ASSERT_NE(scope, nullptr);
  ASSERT_NE(scope->getActual(), nullptr) << "'medium' should resolve upward to module 'medium'";
  const hldb::Module *const medium = scope->getActual<hldb::Module>();
  ASSERT_NE(medium, nullptr);
  EXPECT_EQ(medium->getDefName(), std::string_view{"medium"});
  ASSERT_NE(rhs->getActual(), nullptr) << "'medium.c' is a legal upward reference (Sec 23.8)";
  const hldb::Net *const c = rhs->getActual<hldb::Net>();
  ASSERT_NE(c, nullptr) << "'medium.c' should resolve to medium's net 'c'";
  EXPECT_EQ(c->getName(), std::string_view{"c"});
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
