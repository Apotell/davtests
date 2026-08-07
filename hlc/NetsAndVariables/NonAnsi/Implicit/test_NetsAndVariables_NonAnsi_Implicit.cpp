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

// Validates the UHDM graph produced for tests/NetsAndVariables/NonAnsi/Implicit.sv,
// split out of the combined NetsAndVariablesNonAnsi.sv suite so implicit-net
// handling stands on its own.
//
// Checked:
//   - implicit_net_nonansi (an undeclared identifier used as the LHS of a
//     continuous assignment) is NOT materialized as either a Net or a
//     Variable, matching 6.10--implicit_continuous_assignment -- HLC is a
//     compiler, not an elaborator, so an implicit net is represented only
//     by the RefObj (with a null actual), never by a backing declaration

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/ref_obj.h>
#include <hldb/variable.h>

namespace hlc {

class NonAnsiImplicitTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "Implicit.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getMod() {
    return hldb::findByName<hldb::Module>("nets_and_variables_nonansi", m_design->getAllModules());
  }
};

TEST_F(NonAnsiImplicitTest, ImplicitNetNonansiNotInNets) {
  const hldb::Module *const mod = getMod();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getNets(), nullptr);
  EXPECT_EQ(hldb::findByName<hldb::Net>("implicit_net_nonansi", mod->getNets()), nullptr)
      << "'implicit_net_nonansi' should not appear in vpiNet -- it was implicitly declared";
}

TEST_F(NonAnsiImplicitTest, ImplicitNetNonansiNotInVariables) {
  const hldb::Module *const mod = getMod();
  ASSERT_NE(mod, nullptr);
  EXPECT_EQ(hldb::findByName<hldb::Variable>("implicit_net_nonansi", mod->getVariables()), nullptr)
      << "'implicit_net_nonansi' should not appear in vpiVariables either -- no implicit variable in SV";
}

TEST_F(NonAnsiImplicitTest, ImplicitNetNonansiContAssignHasNoActual) {
  const hldb::Module *const mod = getMod();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getContAssigns(), nullptr);
  ASSERT_FALSE(mod->getContAssigns()->empty());
  const hldb::RefObj *const lhs = mod->getContAssigns()->at(0)->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "implicit_net_nonansi");
  EXPECT_EQ(lhs->getActual(), nullptr) << "'implicit_net_nonansi' is implicitly declared -- no vpiActual";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
